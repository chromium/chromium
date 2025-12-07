// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdbool.h>
#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#include <array>
#include <set>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_driver.h"
#include "third_party/libyuv/include/libyuv.h"

VAStatus FakeTerminate(VADriverContextP ctx) {
  delete static_cast<media::internal::FakeDriver*>(ctx->pDriverData);
  return VA_STATUS_SUCCESS;
}

// Needed to be able to instantiate kCapabilities statically.
#define MAX_CAPABILITY_ATTRIBUTES 5

constexpr auto kSupportedImageFormats =
    std::to_array<VAImageFormat>({{.fourcc = VA_FOURCC_NV12,
                                   .byte_order = VA_LSB_FIRST,
                                   .bits_per_pixel = 12}});

struct Capability {
  VAProfile profile;
  VAEntrypoint entry_point;
  int num_attribs;
  VAConfigAttrib attrib_list[MAX_CAPABILITY_ATTRIBUTES];
};
constexpr auto kCapabilities = std::to_array<Capability>({
    {VAProfileH264ConstrainedBaseline,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileH264Main,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileH264High,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileAV1Profile0,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileVP8Version0_3,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileVP8Version0_3,
     VAEntrypointEncSlice,
     3,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
    {VAProfileVP9Profile0,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileVP9Profile0,
     VAEntrypointEncSlice,
     3,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
    {VAProfileVP9Profile2,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat,
          VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10BPP},
     }},
    {VAProfileVP9Profile2,
     VAEntrypointEncSlice,
     3,
     {
         {VAConfigAttribRTFormat,
          VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10BPP},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
    // JPEG is an image codec, so the encoding entry point is different.
    {VAProfileJPEGBaseline,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileJPEGBaseline,
     VAEntrypointEncPicture,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    // VideoProc is a special silicon area for video/image post processing, e.g.
    // crop, resize, and format conversion.
    {VAProfileNone,
     VAEntrypointVideoProc,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    // H264 codec profiles need the VAConfigAttribEncPackedHeaders attribute for
    // encoding because Chrome will expect it.
    {VAProfileH264ConstrainedBaseline,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileH264ConstrainedBaseline,
     VAEntrypointEncSlice,
     4,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncPackedHeaders, VA_ENC_PACKED_HEADER_NONE},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
    {VAProfileH264Main,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileH264Main,
     VAEntrypointEncSlice,
     4,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncPackedHeaders, VA_ENC_PACKED_HEADER_NONE},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
    {VAProfileH264High,
     VAEntrypointVLD,
     1,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
     }},
    {VAProfileH264High,
     VAEntrypointEncSlice,
     4,
     {
         {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
         {VAConfigAttribRateControl, VA_RC_CQP | VA_RC_CBR},
         {VAConfigAttribEncPackedHeaders, VA_ENC_PACKED_HEADER_NONE},
         {VAConfigAttribEncMaxRefFrames, 1},
     }},
});

/**
 * Original comment:
 * Query supported profiles
 * The caller must provide a "profile_list" array that can hold at
 * least vaMaxNumProfiles() entries. The actual number of profiles
 * returned in "profile_list" is returned in "num_profile".
 */
VAStatus FakeQueryConfigProfiles(VADriverContextP ctx,
                                 VAProfile* profile_list,
                                 int* num_profiles) {
  std::set<VAProfile> unique_profiles;
  for (const Capability& capability : kCapabilities) {
    unique_profiles.insert(capability.profile);
  }

  // SAFETY: The `profile_list` pointer is from the `vaQueryConfigProfiles`
  // function (VA-API C interface). The caller must provide a "profile_list"
  // array that can hold at least vaMaxNumProfiles() entries. There is no
  // way to guarantee the buffer size for the given `profile_list` pointer
  // via the C-style API. The actual number of profiles returned in
  // "profile_list" is returned in "num_profile". For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga198a34eb408790b172710071a248b660
  base::span<VAProfile> profile_list_span = UNSAFE_BUFFERS(
      base::span(profile_list, base::checked_cast<size_t>(ctx->max_profiles)));

  profile_list_span.copy_prefix_from(
      std::vector<VAProfile>(unique_profiles.begin(), unique_profiles.end()));

  *num_profiles = unique_profiles.size();

  return VA_STATUS_SUCCESS;
}

/**
 * Original comment:
 * Query supported entrypoints for a given profile
 * The caller must provide an "entrypoint_list" array that can hold at
 * least vaMaxNumEntrypoints() entries. The actual number of entrypoints
 * returned in "entrypoint_list" is returned in "num_entrypoints".
 */
VAStatus FakeQueryConfigEntrypoints(VADriverContextP ctx,
                                    VAProfile profile,
                                    VAEntrypoint* entrypoint_list,
                                    int* num_entrypoints) {
  // SAFETY: The `entrypoint_list` pointer is from the
  // `vaQueryConfigEntrypoints` function (VA-API C interface). The caller must
  // provide an "entrypoint_list" array that can hold at least
  // vaMaxNumEntrypoints() entries. There is no way to guarantee the buffer size
  // for the given `entrypoint_list` pointer via the C-style API. The actual
  // number of entry points returned in "entrypoint_list" is returned in
  // "num_entrypoints". For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga7c6ec979697dafc172123c5d3ad80d8e
  base::span<VAEntrypoint> entrypoint_list_span = UNSAFE_BUFFERS(base::span(
      entrypoint_list, base::checked_cast<size_t>(ctx->max_entrypoints)));

  int actual_num_entrypoints = 0;
  for (const Capability& capability : kCapabilities) {
    if (capability.profile == profile) {
      entrypoint_list_span[actual_num_entrypoints++] = capability.entry_point;
    }
  }

  *num_entrypoints = actual_num_entrypoints;
  return VA_STATUS_SUCCESS;
}

/**
 * Original comment:
 * Get attributes for a given profile/entrypoint pair
 * The caller must provide an "attrib_list" with all attributes to be
 * retrieved.  Upon return, the attributes in "attrib_list" have been
 * updated with their value.  Unknown attributes or attributes that are
 * not supported for the given profile/entrypoint pair will have their
 * value set to VA_ATTRIB_NOT_SUPPORTED
 */
VAStatus FakeGetConfigAttributes(VADriverContextP ctx,
                                 VAProfile profile,
                                 VAEntrypoint entrypoint,
                                 VAConfigAttrib* attrib_list,
                                 int num_attribs) {
  // SAFETY: The `attrib_list` pointer is from the `vaGetConfigAttributes`
  // function (VA-API C interface). The caller must provide an "attrib_list".
  // There is no way to guarantee the buffer size for the given `attrib_list`
  // pointer via the C-style API. Upon return, the attributes in "attrib_list"
  // have been updated. For more details, see:
  // https://intel.github.io/libva/group__api__core.html#gae51cad2e388d6cc63ce3d4221798f9fd
  base::span<VAConfigAttrib> attrib_list_span = UNSAFE_BUFFERS(
      base::span(attrib_list, base::checked_cast<size_t>(num_attribs)));
  // Clear the |attrib_list|: sometimes it's not initialized.
  for (VAConfigAttrib& attrib : attrib_list_span) {
    attrib.value = VA_ATTRIB_NOT_SUPPORTED;
  }

  // First, try to find the |profile| and |entrypoint| entry in kCapabilities.
  // If found, search for each entry in the input |attrib_list| (usually many)
  // in kCapabilities[i]'s |attrib_list| (usually few), and, if found, update
  // its |value|.
  bool profile_found = false;
  for (const Capability& capability : kCapabilities) {
    profile_found = capability.profile == profile || profile_found;
    if (!(capability.profile == profile &&
          capability.entry_point == entrypoint)) {
      continue;
    }

    base::span<const VAConfigAttrib> capability_attrib_list_span =
        base::span(capability.attrib_list)
            .first(base::checked_cast<size_t>(capability.num_attribs));

    for (const VAConfigAttrib& capability_attrib :
         capability_attrib_list_span) {
      for (VAConfigAttrib& attrib : attrib_list_span) {
        if (attrib.type == capability_attrib.type) {
          attrib.value = capability_attrib.value;
          break;
        }
      }
    }
    return VA_STATUS_SUCCESS;
  }
  return profile_found ? VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT
                       : VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

VAStatus FakeCreateConfig(VADriverContextP ctx,
                          VAProfile profile,
                          VAEntrypoint entrypoint,
                          VAConfigAttrib* attrib_list,
                          int num_attribs,
                          VAConfigID* config_id) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);
  // SAFETY: The attrib_list pointer and num_attribs size are from the VA-API C
  // function vaCreateConfig (via VADriverVTable). This C API provides a raw
  // pointer and size, but offers no guarantee that num_attribs is a safe bound
  // for accessing attrib_list. For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga9ff7833d425406cb1834c783b0a47652
  base::span<VAConfigAttrib> attribs_list_span = UNSAFE_BUFFERS(
      base::span(attrib_list, base::checked_cast<size_t>(num_attribs)));

  *config_id = VA_INVALID_ID;
  bool profile_found = false;
  for (const Capability& capability : kCapabilities) {
    profile_found = capability.profile == profile || profile_found;
    if (!(capability.profile == profile &&
          capability.entry_point == entrypoint)) {
      continue;
    }

    std::vector<VAConfigAttrib> attribs;

    // Checks that the attrib_list is supported by the profile. Assumes the
    // attributes can be in any order.
    for (const VAConfigAttrib& attrib : attribs_list_span) {
      bool attrib_supported = false;

      base::span<const VAConfigAttrib> capability_attrib_list_span =
          base::span(capability.attrib_list)
              .first(base::checked_cast<size_t>(capability.num_attribs));

      for (const VAConfigAttrib& capability_attrib :
           capability_attrib_list_span) {
        if (capability_attrib.type != attrib.type) {
          continue;
        }
        // Note that it's not enough to AND the value in |kCapabilities| against
        // the value provided by the application. We also need to allow for
        // equality. The reason is that there are some attributes that allow a
        // value of 0 (e.g., VA_ENC_PACKED_HEADER_NONE for
        // VAConfigAttribEncPackedHeaders).
        attrib_supported = (capability_attrib.value & attrib.value) ||
                           (capability_attrib.value == attrib.value);
        // TODO(b/258275488): Handle duplicate attributes in attrib_list.
        if (attrib_supported) {
          attribs.push_back(attrib);
          break;
        }
      }
      if (!attrib_supported) {
        return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
      }
    }

    for (const VAConfigAttrib& capability_attrib : capability.attrib_list) {
      if (std::find_if(attribs.begin(), attribs.end(),
                       [&capability_attrib](const VAConfigAttrib& attrib) {
                         return attrib.type == capability_attrib.type;
                       }) == attribs.end()) {
        // TODO(b/258275488): Handle default values correctly. Currently,
        // capability_attrib only contains possible values for a given
        // attribute, not the default value.
        attribs.push_back(capability_attrib);
      }
    }

    *config_id = fdrv->CreateConfig(profile, entrypoint, std::move(attribs));

    return VA_STATUS_SUCCESS;
  }

  return profile_found ? VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT
                       : VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
}

/**
 * Original comment:
 * Query all attributes for a given configuration
 * The profile of the configuration is returned in "profile"
 * The entrypoint of the configuration is returned in "entrypoint"
 * The caller must provide an "attrib_list" array that can hold at least
 * vaMaxNumConfigAttributes() entries. The actual number of attributes
 * returned in "attrib_list" is returned in "num_attribs"
 */
// Misleading function name: it queries |profile|, |entrypoint| and  attributes
// (|attrib_list|) for the given |config_id|.
VAStatus FakeQueryConfigAttributes(VADriverContextP ctx,
                                   VAConfigID config_id,
                                   VAProfile* profile,
                                   VAEntrypoint* entrypoint,
                                   VAConfigAttrib* attrib_list,
                                   int* num_attribs) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  const media::internal::FakeConfig& fconfig = fdrv->GetConfig(config_id);

  *profile = fconfig.GetProfile();
  *entrypoint = fconfig.GetEntrypoint();

  size_t config_attribs_size = fconfig.GetConfigAttribs().size();
  // SAFETY: The `attrib_list` pointer originates from
  // the`vaQueryConfigAttributes` function (VA-API C interface). The caller must
  // provide an "attrib_list" array that can hold at least
  // vaMaxNumConfigAttributes() entries. This C-style API provides a raw pointer
  // and a size, but there is no way to guarantee that the provided
  // `num_attribs` is a valid buffer size for the given `attrib_list` pointer.
  // For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga593da1618f3495a3f3ac13853a64794c
  base::span<VAConfigAttrib> attrib_list_span =
      UNSAFE_BUFFERS(base::span(attrib_list, config_attribs_size));
  attrib_list_span.copy_from(fconfig.GetConfigAttribs());
  *num_attribs = config_attribs_size;

  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyConfig(VADriverContextP ctx, VAConfigID config_id) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  fdrv->DestroyConfig(config_id);

  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateSurfaces(VADriverContextP ctx,
                            int width,
                            int height,
                            int format,
                            int num_surfaces,
                            VASurfaceID* surfaces) {
  NOTREACHED();
}

VAStatus FakeDestroySurfaces(VADriverContextP ctx,
                             VASurfaceID* surface_list,
                             int num_surfaces) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);
  // SAFETY: The `surface_list` pointer is from the `vaDestroySurfaces` function
  // (VA-API C interface). This C-style API provides a raw pointer and a size
  // (`num_surfaces`). There is no way to guarantee that the provided
  // `num_surfaces` is a valid buffer size for the given `surface_list` array
  // of surfaces to destroy. For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga108b11751ff3e1113732780bb5b3d547
  base::span<VASurfaceID> surface_list_span = UNSAFE_BUFFERS(
      base::span(surface_list, base::checked_cast<size_t>(num_surfaces)));
  for (unsigned int& surface : surface_list_span) {
    fdrv->DestroySurface(surface);
  }

  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateContext(VADriverContextP ctx,
                           VAConfigID config_id,
                           int picture_width,
                           int picture_height,
                           int flag,
                           VASurfaceID* render_targets,
                           int num_render_targets,
                           VAContextID* context) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->ConfigExists(config_id));
  // SAFETY: The `render_targets` pointer is from the `vaCreateContext` function
  // (VA-API C interface). This C-style API provides a raw pointer and a size
  // (`num_render_targets`). There is no way to guarantee that the provided
  // `num_render_targets` is a valid buffer size for the given `render_targets`
  // pointer. `render_targets`: a hint for render targets (surfaces).
  // `num_render_targets`: number of render targets in the array.
  // For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga7a0e774a793545058d1a311bed9bb8cf
  base::span<VASurfaceID> render_targets_span = UNSAFE_BUFFERS(base::span(
      render_targets, base::checked_cast<size_t>(num_render_targets)));
  for (const VASurfaceID& render_target : render_targets_span) {
    CHECK(fdrv->SurfaceExists(render_target));
  }

  *context =
      fdrv->CreateContext(config_id, picture_width, picture_height, flag,
                          std::vector<VASurfaceID>(render_targets_span.begin(),
                                                   render_targets_span.end()));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyContext(VADriverContextP ctx, VAContextID context) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  fdrv->DestroyContext(context);

  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateBuffer(VADriverContextP ctx,
                          VAContextID context,
                          VABufferType type,
                          unsigned int size,
                          unsigned int num_elements,
                          void* data,
                          VABufferID* buf_id) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->ContextExists(context));

  *buf_id = fdrv->CreateBuffer(context, type, /*size_per_element=*/size,
                               num_elements, data);

  return VA_STATUS_SUCCESS;
}

VAStatus FakeBufferSetNumElements(VADriverContextP ctx,
                                  VABufferID buf_id,
                                  unsigned int num_elements) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeMapBuffer(VADriverContextP ctx, VABufferID buf_id, void** pbuf) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);
  *pbuf = fdrv->GetBuffer(buf_id).GetData().data();
  return VA_STATUS_SUCCESS;
}

VAStatus FakeUnmapBuffer(VADriverContextP ctx, VABufferID buf_id) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyBuffer(VADriverContextP ctx, VABufferID buffer_id) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  fdrv->DestroyBuffer(buffer_id);

  return VA_STATUS_SUCCESS;
}

VAStatus FakeBeginPicture(VADriverContextP ctx,
                          VAContextID context,
                          VASurfaceID render_target) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(render_target));
  CHECK(fdrv->ContextExists(context));

  fdrv->GetContext(context).BeginPicture(fdrv->GetSurface(render_target));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeRenderPicture(VADriverContextP ctx,
                           VAContextID context,
                           VABufferID* buffers,
                           int num_buffers) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->ContextExists(context));
  // SAFETY: The `buffers` pointer is from the `vaRenderPicture` function
  // (VA-API C interface). This C-style API provides a raw pointer and a size
  // (`num_buffers`). There is no way to guarantee that the provided
  // `num_buffers` is a valid buffer size for the given `buffers` pointer.
  // For more details, see
  // https://intel.github.io/libva/group__api__core.html#ga3facc622a14fc901d5d44dcda845cb6f
  base::span<VABufferID> buffers_span = UNSAFE_BUFFERS(
      base::span(buffers, base::checked_cast<size_t>(num_buffers)));
  std::vector<raw_ptr<const media::internal::FakeBuffer>> buffer_list;
  for (const VABufferID& buffer_id : buffers_span) {
    CHECK(fdrv->BufferExists(buffer_id));
    buffer_list.push_back(&(fdrv->GetBuffer(buffer_id)));
  }

  fdrv->GetContext(context).RenderPicture(buffer_list);

  return VA_STATUS_SUCCESS;
}

VAStatus FakeEndPicture(VADriverContextP ctx, VAContextID context) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->ContextExists(context));

  fdrv->GetContext(context).EndPicture();

  return VA_STATUS_SUCCESS;
}

VAStatus FakeSyncSurface(VADriverContextP ctx, VASurfaceID render_target) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(render_target));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeQuerySurfaceStatus(VADriverContextP ctx,
                                VASurfaceID render_target,
                                VASurfaceStatus* status) {
  NOTREACHED();
}

VAStatus FakePutSurface(VADriverContextP ctx,
                        VASurfaceID surface,
                        void* draw,
                        short srcx,
                        short srcy,
                        unsigned short srcw,
                        unsigned short srch,
                        short destx,
                        short desty,
                        unsigned short destw,
                        unsigned short desth,
                        VARectangle* cliprects,
                        unsigned int number_cliprects,
                        unsigned int flags) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(surface));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeQueryImageFormats(VADriverContextP ctx,
                               VAImageFormat* format_list,
                               int* num_formats) {
  // SAFETY: The `format_list` pointer is from the `vaQueryImageFormats`
  // function (VA-API C interface). The API requires that `format_list` be able
  // to hold vaMaxNumImageFormats(). The caller must provide a "format_list"
  // array that can hold at least vaMaxNumImageFormats() entries. The number of
  // formats returned is in "num_formats". For more details, see:
  // https://intel.github.io/libva/group__api__core.html#gacaafd538e7a9c79fdd9753c4243be3b8
  base::span<VAImageFormat> format_span = UNSAFE_BUFFERS(base::span(
      format_list, base::checked_cast<size_t>(ctx->max_image_formats)));

  int actual_num_formats = 0;
  for (const VAImageFormat& format : kSupportedImageFormats) {
    format_span[actual_num_formats++] = format;
  }

  *num_formats = actual_num_formats;

  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateImage(VADriverContextP ctx,
                         VAImageFormat* format,
                         int width,
                         int height,
                         VAImage* image) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  fdrv->CreateImage(*format, width, height, image);
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyImage(VADriverContextP ctx, VAImageID image) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  fdrv->DestroyImage(image);
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSetImagePalette(VADriverContextP ctx,
                             VAImageID image,
                             unsigned char* palette) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeGetImage(VADriverContextP ctx,
                      VASurfaceID surface,
                      int x,
                      int y,
                      unsigned int width,
                      unsigned int height,
                      VAImageID image) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(surface));

  const media::internal::FakeSurface& fake_surface = fdrv->GetSurface(surface);

  CHECK(fdrv->ImageExists(image));

  // TODO(b/316609501): Look into replacing this and making this function
  // operate the same for both testing and non-testing environments.
  if (!fake_surface.GetMappedBO().IsValid()) {
    return VA_STATUS_SUCCESS;
  }

  // Chrome should only request images starting at (0, 0).
  CHECK_EQ(x, 0);
  CHECK_EQ(y, 0);
  CHECK_LE(width, fake_surface.GetWidth());
  CHECK_LE(height, fake_surface.GetHeight());

  // Chrome should only ask the fake driver for images sourced from NV12
  // surfaces.
  CHECK_EQ(fake_surface.GetVAFourCC(), static_cast<uint32_t>(VA_FOURCC_NV12));

  const media::internal::ScopedBOMapping::ScopedAccess mapped_bo =
      fake_surface.GetMappedBO().BeginAccess();

  const media::internal::FakeImage& fake_image = fdrv->GetImage(image);

  // Chrome should only ask the fake driver to download NV12 surfaces onto NV12
  // images.
  CHECK_EQ(fake_image.GetFormat().fourcc,
           static_cast<uint32_t>(VA_FOURCC_NV12));

  // The image dimensions must be large enough to contain the surface.
  CHECK_GE(base::checked_cast<unsigned int>(fake_image.GetWidth()), width);
  CHECK_GE(base::checked_cast<unsigned int>(fake_image.GetHeight()), height);

  base::span<uint8_t> image_data = fake_image.GetBuffer().GetData();
  uint8_t* const dst_y_addr = &image_data[fake_image.GetPlaneOffset(0)];
  const int dst_y_stride =
      base::checked_cast<int>(fake_image.GetPlaneStride(0));

  uint8_t* const dst_uv_addr = &image_data[fake_image.GetPlaneOffset(1)];
  const int dst_uv_stride =
      base::checked_cast<int>(fake_image.GetPlaneStride(1));

  const int copy_result = libyuv::NV12Copy(
      /*src_y=*/mapped_bo.GetData(0),
      /*src_stride_y=*/base::checked_cast<int>(mapped_bo.GetStride(0)),
      /*src_uv=*/mapped_bo.GetData(1),
      /*src_stride_uv=*/base::checked_cast<int>(mapped_bo.GetStride(1)),
      /*dst_y=*/dst_y_addr,
      /*dst_stride_y=*/dst_y_stride,
      /*dst_uv=*/dst_uv_addr,
      /*dst_stride_uv=*/dst_uv_stride,
      /*width=*/width,
      /*height=*/height);

  CHECK_EQ(copy_result, 0);

  return VA_STATUS_SUCCESS;
}

VAStatus FakePutImage(VADriverContextP ctx,
                      VASurfaceID surface,
                      VAImageID image,
                      int src_x,
                      int src_y,
                      unsigned int src_width,
                      unsigned int src_height,
                      int dest_x,
                      int dest_y,
                      unsigned int dest_width,
                      unsigned int dest_height) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(surface));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeDeriveImage(VADriverContextP ctx,
                         VASurfaceID surface,
                         VAImage* image) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->SurfaceExists(surface));

  return VA_STATUS_SUCCESS;
}

VAStatus FakeQuerySubpictureFormats(VADriverContextP ctx,
                                    VAImageFormat* format_list,
                                    unsigned int* flags,
                                    unsigned int* num_formats) {
  *num_formats = 0;
  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateSubpicture(VADriverContextP ctx,
                              VAImageID image,
                              VASubpictureID* subpicture) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroySubpicture(VADriverContextP ctx,
                               VASubpictureID subpicture) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSetSubpictureImage(VADriverContextP ctx,
                                VASubpictureID subpicture,
                                VAImageID image) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSetSubpictureChromakey(VADriverContextP ctx,
                                    VASubpictureID subpicture,
                                    unsigned int chromakey_min,
                                    unsigned int chromakey_max,
                                    unsigned int chromakey_mask) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSetSubpictureGlobalAlpha(VADriverContextP ctx,
                                      VASubpictureID subpicture,
                                      float global_alpha) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeAssociateSubpicture(VADriverContextP ctx,
                                 VASubpictureID subpicture,
                                 VASurfaceID* target_surfaces,
                                 int num_surfaces,
                                 int16_t src_x,
                                 int16_t src_y,
                                 uint16_t src_width,
                                 uint16_t src_height,
                                 int16_t dest_x,
                                 int16_t dest_y,
                                 uint16_t dest_width,
                                 uint16_t dest_height,
                                 uint32_t flags) {
  NOTREACHED();
}

VAStatus FakeDeassociateSubpicture(VADriverContextP ctx,
                                   VASubpictureID subpicture,
                                   VASurfaceID* target_surfaces,
                                   int num_surfaces) {
  NOTREACHED();
}

VAStatus FakeQueryDisplayAttributes(VADriverContextP ctx,
                                    VADisplayAttribute* attr_list,
                                    int* num_attributes) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeGetDisplayAttributes(VADriverContextP ctx,
                                  VADisplayAttribute* attr_list,
                                  int num_attributes) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSetDisplayAttributes(VADriverContextP ctx,
                                  VADisplayAttribute* attr_list,
                                  int num_attributes) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeQuerySurfaceAttributes(VADriverContextP ctx,
                                    VAConfigID config,
                                    VASurfaceAttrib* attribs,
                                    unsigned int* num_attribs) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  CHECK(fdrv->ConfigExists(config));

  // This function is called once with |attribs| NULL to dimension output. The
  // second time, |num_attribs| must be larger than kMaxNumSurfaceAttributes.
  // See the original documentation:
  // "The attrib_list array is allocated by the user and num_attribs shall be
  // initialized to the number of allocated elements in that array. Upon
  // successful return, the actual number of attributes will be overwritten into
  // num_attribs. Otherwise, VA_STATUS_ERROR_MAX_NUM_EXCEEDED is returned and
  // num_attribs is adjusted to the number of elements that would be returned if
  // enough space was available."
  const unsigned int kMaxNumSurfaceAttributes = 32;
  if (attribs == nullptr) {
    *num_attribs = kMaxNumSurfaceAttributes;
    return VA_STATUS_SUCCESS;
  }
  if (*num_attribs < kMaxNumSurfaceAttributes) {
    *num_attribs = kMaxNumSurfaceAttributes;
    return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
  }

  // SAFETY: The `attribs` pointer is from the
  // `vaQuerySurfaceAttributes` function (VA-API C interface). The `attribs`
  // array is allocated by the user. This C-style API provides a raw pointer and
  // a size (`num_attribs`), but there is no way to guarantee that the provided
  // `num_attribs` is a valid buffer size. If enough space was available, the
  // actual number of attributes will be returned in `num_attribs`. It is
  // perfectly valid to pass NULL to the `attribs` argument to determine the
  // number of elements that need to be allocated. For more details, see:
  // https://intel.github.io/libva/group__api__core.html#ga6b10b88a628c56377268714cc72090ce
  base::span<VASurfaceAttrib> attribs_span =
      UNSAFE_BUFFERS(base::span(attribs, kMaxNumSurfaceAttributes));
  // |attribs| may have a single VASurfaceAttribPixelFormat set for querying
  // support for a given pixel format. Chrome doesn't support it, so we verify
  // all input types are zero (VASurfaceAttribNone).
  for (const VASurfaceAttrib& attrib : attribs_span) {
    if (attrib.type != VASurfaceAttribNone) {
      *num_attribs = 0;
      return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
    }
  }

  int i = 0;
  attribs_span[i].type = VASurfaceAttribPixelFormat;
  attribs_span[i].value.type = VAGenericValueTypeInteger;
  attribs_span[i].flags =
      VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
  attribs_span[i].value.value.i = VA_FOURCC_NV12;
  i++;

  attribs_span[i].type = VASurfaceAttribPixelFormat;
  attribs_span[i].value.type = VAGenericValueTypeInteger;
  attribs_span[i].flags =
      VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
  attribs_span[i].value.value.i = VA_FOURCC_YV12;
  i++;

  attribs_span[i].type = VASurfaceAttribMaxWidth;
  attribs_span[i].value.type = VAGenericValueTypeInteger;
  attribs_span[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
  attribs_span[i].value.value.i = 1024;
  i++;

  attribs_span[i].type = VASurfaceAttribMaxHeight;
  attribs_span[i].value.type = VAGenericValueTypeInteger;
  attribs_span[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
  attribs_span[i].value.value.i = 1024;
  i++;

  *num_attribs = i;
  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateSurfaces2(VADriverContextP ctx,
                             unsigned int format,
                             unsigned int width,
                             unsigned int height,
                             VASurfaceID* surfaces,
                             unsigned int num_surfaces,
                             VASurfaceAttrib* attrib_list,
                             unsigned int num_attribs) {
  media::internal::FakeDriver* fdrv =
      static_cast<media::internal::FakeDriver*>(ctx->pDriverData);

  // SAFETY: The `surfaces` and `attrib_list` pointers are from the
  // `vaCreateSurfaces` function (VA-API C interface). This C-style API
  // provides raw pointers and sizes. There is no way to guarantee that the
  // provided sizes are valid buffer sizes for the given pointers (`surfaces`
  // and `attrib_list`). The optional list of attributes shall be constructed
  // based on vaQuerySurfaceAttributes().
  // For more details, see:
  // https://intel.github.io/libva/group__api__core.html#gac970ea0eec412326667549f58c44129b
  base::span<VASurfaceID> surfaces_span =
      UNSAFE_BUFFERS(base::span(surfaces, num_surfaces));
  base::span<VASurfaceAttrib> attrib_list_span =
      UNSAFE_BUFFERS(base::span(attrib_list, num_attribs));
  std::vector<VASurfaceAttrib> attrib_list_vector(attrib_list_span.begin(),
                                                  attrib_list_span.end());
  for (unsigned int& surface : surfaces_span) {
    surface = fdrv->CreateSurface(format, width, height, attrib_list_vector);
  }

  return VA_STATUS_SUCCESS;
}

#define MAX_PROFILES 12
#define MAX_ENTRYPOINTS 8
#define MAX_CONFIG_ATTRIBUTES 32
#if MAX_CAPABILITY_ATTRIBUTES >= MAX_CONFIG_ATTRIBUTES
#error "MAX_CAPABILITY_ATTRIBUTES should be smaller than MAX_CONFIG_ATTRIBUTES"
#endif
#define MAX_IMAGE_FORMATS 10
#define MAX_SUBPIC_FORMATS 6

#define DLL_EXPORT __attribute__((visibility("default")))

extern "C" VAStatus DLL_EXPORT __vaDriverInit_1_0(VADriverContextP ctx) {
  struct VADriverVTable* const vtable = ctx->vtable;

  ctx->version_major = VA_MAJOR_VERSION;
  ctx->version_minor = VA_MINOR_VERSION;
  ctx->str_vendor = "Chromium fake libva driver";
  CHECK(ctx->drm_state);

  ctx->pDriverData = new media::internal::FakeDriver(
      (static_cast<drm_state*>(ctx->drm_state))->fd);

  ctx->max_profiles = MAX_PROFILES;
  ctx->max_entrypoints = MAX_ENTRYPOINTS;
  ctx->max_attributes = MAX_CONFIG_ATTRIBUTES;
  ctx->max_image_formats = MAX_IMAGE_FORMATS;
  ctx->max_subpic_formats = MAX_SUBPIC_FORMATS;
  ctx->max_display_attributes = 1;

  vtable->vaTerminate = FakeTerminate;
  vtable->vaQueryConfigEntrypoints = FakeQueryConfigEntrypoints;
  vtable->vaQueryConfigProfiles = FakeQueryConfigProfiles;
  vtable->vaQueryConfigAttributes = FakeQueryConfigAttributes;
  vtable->vaCreateConfig = FakeCreateConfig;
  vtable->vaDestroyConfig = FakeDestroyConfig;
  vtable->vaGetConfigAttributes = FakeGetConfigAttributes;
  vtable->vaCreateSurfaces = FakeCreateSurfaces;
  vtable->vaDestroySurfaces = FakeDestroySurfaces;
  vtable->vaCreateContext = FakeCreateContext;
  vtable->vaDestroyContext = FakeDestroyContext;
  vtable->vaCreateBuffer = FakeCreateBuffer;
  vtable->vaBufferSetNumElements = FakeBufferSetNumElements;
  vtable->vaMapBuffer = FakeMapBuffer;
  vtable->vaUnmapBuffer = FakeUnmapBuffer;
  vtable->vaDestroyBuffer = FakeDestroyBuffer;
  vtable->vaBeginPicture = FakeBeginPicture;
  vtable->vaRenderPicture = FakeRenderPicture;
  vtable->vaEndPicture = FakeEndPicture;
  vtable->vaSyncSurface = FakeSyncSurface;
  vtable->vaQuerySurfaceStatus = FakeQuerySurfaceStatus;
  vtable->vaPutSurface = FakePutSurface;

  vtable->vaQueryImageFormats = FakeQueryImageFormats;
  vtable->vaCreateImage = FakeCreateImage;
  vtable->vaDeriveImage = FakeDeriveImage;
  vtable->vaDestroyImage = FakeDestroyImage;
  vtable->vaSetImagePalette = FakeSetImagePalette;
  vtable->vaGetImage = FakeGetImage;
  vtable->vaPutImage = FakePutImage;

  vtable->vaQuerySubpictureFormats = FakeQuerySubpictureFormats;
  vtable->vaCreateSubpicture = FakeCreateSubpicture;
  vtable->vaDestroySubpicture = FakeDestroySubpicture;
  vtable->vaSetSubpictureImage = FakeSetSubpictureImage;
  vtable->vaSetSubpictureChromakey = FakeSetSubpictureChromakey;
  vtable->vaSetSubpictureGlobalAlpha = FakeSetSubpictureGlobalAlpha;
  vtable->vaAssociateSubpicture = FakeAssociateSubpicture;
  vtable->vaDeassociateSubpicture = FakeDeassociateSubpicture;
  vtable->vaQueryDisplayAttributes = FakeQueryDisplayAttributes;
  vtable->vaGetDisplayAttributes = FakeGetDisplayAttributes;
  vtable->vaSetDisplayAttributes = FakeSetDisplayAttributes;

  // Not needed by va_openDriver(), but by Chrome to enumerate profiles and
  // other advanced functionality.
  vtable->vaQuerySurfaceAttributes = FakeQuerySurfaceAttributes;
  vtable->vaCreateSurfaces2 = FakeCreateSurfaces2;

  return VA_STATUS_SUCCESS;
}
