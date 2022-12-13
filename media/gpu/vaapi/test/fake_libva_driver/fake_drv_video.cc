// Copyright 2020 The Chromium OS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdbool.h>
#include <va/va.h>
#include <va/va_backend.h>

#include <set>

VAStatus FakeTerminate(VADriverContextP ctx) {
  return VA_STATUS_SUCCESS;
}

// Needed to be able to instantiate kCapabilities statically.
#define MAX_CAPABILITY_ATTRIBUTES 5

struct Capability {
  VAProfile profile;
  VAEntrypoint entry_point;
  int num_attribs;
  VAConfigAttrib attrib_list[MAX_CAPABILITY_ATTRIBUTES];
};
const struct Capability kCapabilities[] = {
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
     }}};

const size_t kCapabilitiesSize =
    sizeof(kCapabilities) / sizeof(struct Capability);

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
  int i = 0;

  std::set<VAProfile> unique_profiles;
  for (auto& capability : kCapabilities)
    unique_profiles.insert(capability.profile);

  for (auto profile : unique_profiles) {
    profile_list[i] = profile;
    i++;
  }

  *num_profiles = i;

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
  *num_entrypoints = 0;
  for (const auto& capability : kCapabilities) {
    if (capability.profile == profile)
      entrypoint_list[(*num_entrypoints)++] = capability.entry_point;
  }
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
  // First, try to find the |profile| and |entrypoint| entry in kCapabilities.
  // If found, search for each entry in the input |attrib_list| (usually many)
  // in kCapabilities[i]'s |attrib_list| (usually few), and, if found, update
  // its |value|.
  bool profile_found = false;
  for (const auto& capability : kCapabilities) {
    profile_found = capability.profile == profile || profile_found;
    if (!(capability.profile == profile &&
          capability.entry_point == entrypoint)) {
      continue;
    }

    // Clear the |attrib_list|: sometimes it's not initialized.
    for (int attrib = 0; attrib < num_attribs; attrib++)
      attrib_list[attrib].value = VA_ATTRIB_NOT_SUPPORTED;

    for (int j = 0; j < capability.num_attribs; j++) {
      for (int n = 0; n < num_attribs; n++) {
        if (capability.attrib_list[j].type == attrib_list[n].type) {
          attrib_list[n].value = capability.attrib_list[j].value;
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
  *config_id = VA_INVALID_ID;
  bool profile_found = false;
  for (size_t i = 0; i < kCapabilitiesSize; ++i) {
    profile_found = kCapabilities[i].profile == profile || profile_found;
    if (!(kCapabilities[i].profile == profile &&
          kCapabilities[i].entry_point == entrypoint)) {
      continue;
    }
    // Checks that the attrib_list is supported by the profile. Assumes the
    // attributes can be in any order.
    for (int k = 0; k < num_attribs; k++) {
      bool attrib_supported = false;
      for (int j = 0; j < kCapabilities[i].num_attribs; j++) {
        if (kCapabilities[i].attrib_list[j].type != attrib_list[k].type)
          continue;
        // Note that it's not enough to AND the value in |kCapabilities| against
        // the value provided by the application. We also need to allow for
        // equality. The reason is that there are some attributes that allow a
        // value of 0 (e.g., VA_ENC_PACKED_HEADER_NONE for
        // VAConfigAttribEncPackedHeaders).
        attrib_supported =
            (kCapabilities[i].attrib_list[j].value & attrib_list[k].value) ||
            (kCapabilities[i].attrib_list[j].value == attrib_list[k].value);
        if (attrib_supported)
          break;
      }
      if (!attrib_supported) {
        return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
      }
    }

    // |config_id| is also the index in kCapabilities, to simplify things.
    *config_id = i;
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
  // |config_id| is also the index in kCapabilities, to simplify things.
  if (config_id >= kCapabilitiesSize)
    return VA_STATUS_ERROR_INVALID_CONFIG;

  *profile = kCapabilities[config_id].profile;
  *entrypoint = kCapabilities[config_id].entry_point;
  *num_attribs = kCapabilities[config_id].num_attribs;
  for (int j = 0; j < kCapabilities[config_id].num_attribs; j++) {
    attrib_list[j].type = kCapabilities[config_id].attrib_list[j].type;
    attrib_list[j].value = kCapabilities[config_id].attrib_list[j].value;
  }
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyConfig(VADriverContextP ctx, VAConfigID config_id) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateSurfaces(VADriverContextP ctx,
                            int width,
                            int height,
                            int format,
                            int num_surfaces,
                            VASurfaceID* surfaces) {
  for (int index = 0; index < num_surfaces; ++index)
    surfaces[index] = index;

  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroySurfaces(VADriverContextP ctx,
                             VASurfaceID* surface_list,
                             int num_surfaces) {
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
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyContext(VADriverContextP ctx, VAContextID context) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateBuffer(VADriverContextP ctx,
                          VAContextID context,
                          VABufferType type,
                          unsigned int size,
                          unsigned int num_elements,
                          void* data,
                          VABufferID* buf_id) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeBufferSetNumElements(VADriverContextP ctx,
                                  VABufferID buf_id,
                                  unsigned int num_elements) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeMapBuffer(VADriverContextP ctx, VABufferID buf_id, void** pbuf) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeUnmapBuffer(VADriverContextP ctx, VABufferID buf_id) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyBuffer(VADriverContextP ctx, VABufferID buffer_id) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeBeginPicture(VADriverContextP ctx,
                          VAContextID context,
                          VASurfaceID render_target) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeRenderPicture(VADriverContextP ctx,
                           VAContextID context,
                           VABufferID* buffers,
                           int num_buffers) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeEndPicture(VADriverContextP ctx, VAContextID context) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeSyncSurface(VADriverContextP ctx, VASurfaceID render_target) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeQuerySurfaceStatus(VADriverContextP ctx,
                                VASurfaceID render_target,
                                VASurfaceStatus* status) {
  return VA_STATUS_SUCCESS;
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
  return VA_STATUS_SUCCESS;
}

VAStatus FakeQueryImageFormats(VADriverContextP ctx,
                               VAImageFormat* format_list,
                               int* num_formats) {
  *num_formats = 0;
  return VA_STATUS_SUCCESS;
}

VAStatus FakeCreateImage(VADriverContextP ctx,
                         VAImageFormat* format,
                         int width,
                         int height,
                         VAImage* image) {
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDestroyImage(VADriverContextP ctx, VAImageID image) {
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
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDeriveImage(VADriverContextP ctx,
                         VASurfaceID surface,
                         VAImage* image) {
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
  return VA_STATUS_SUCCESS;
}

VAStatus FakeDeassociateSubpicture(VADriverContextP ctx,
                                   VASubpictureID subpicture,
                                   VASurfaceID* target_surfaces,
                                   int num_surfaces) {
  return VA_STATUS_SUCCESS;
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

  // |attribs| may have a single VASurfaceAttribPixelFormat set for querying
  // support for a given pixel format. Chrome doesn't support it, so we verify
  // all input types are zero (VASurfaceAttribNone).
  for (size_t i = 0; i < kMaxNumSurfaceAttributes; ++i) {
    if (attribs[i].type != VASurfaceAttribNone) {
      *num_attribs = 0;
      return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;
    }
  }

  int i = 0;
  attribs[i].type = VASurfaceAttribPixelFormat;
  attribs[i].value.type = VAGenericValueTypeInteger;
  attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
  attribs[i].value.value.i = VA_FOURCC_NV12;
  i++;

  attribs[i].type = VASurfaceAttribPixelFormat;
  attribs[i].value.type = VAGenericValueTypeInteger;
  attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
  attribs[i].value.value.i = VA_FOURCC_YV12;
  i++;

  attribs[i].type = VASurfaceAttribMaxWidth;
  attribs[i].value.type = VAGenericValueTypeInteger;
  attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
  attribs[i].value.value.i = 1024;
  i++;

  attribs[i].type = VASurfaceAttribMaxHeight;
  attribs[i].value.type = VAGenericValueTypeInteger;
  attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
  attribs[i].value.value.i = 1024;
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
  return VA_STATUS_SUCCESS;
}

#define MAX_PROFILES 8
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
  ctx->str_vendor = "libfake";

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
