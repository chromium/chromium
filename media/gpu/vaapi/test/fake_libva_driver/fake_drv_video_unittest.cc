// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <va/va.h>
#include <va/va_drm.h>
#include <xf86drm.h>

#include <algorithm>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/types/fixed_array.h"
#include "media/gpu/vaapi/va_stubs.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
using media_gpu_vaapi::kModuleVa_prot;
#endif

using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
using media_gpu_vaapi::StubPathMap;

class FakeDriverTest : public testing::Test {
 public:
  FakeDriverTest() = default;
  FakeDriverTest(const FakeDriverTest&) = delete;
  FakeDriverTest& operator=(const FakeDriverTest&) = delete;
  ~FakeDriverTest() override = default;

  void SetUp() override {
    constexpr char kRenderNodeFilePattern[] = "/dev/dri/renderD%d";
    // This loop ends on either the first card that does not exist or the first
    // render node that is not vgem.
    for (int i = 128;; i++) {
      base::FilePath dev_path(FILE_PATH_LITERAL(
          base::StringPrintf(kRenderNodeFilePattern, i).c_str()));
      base::File drm_file =
          base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
      if (!drm_file.IsValid()) {
        break;
      }
      // Skip the virtual graphics memory manager device.
      drmVersionPtr version = drmGetVersion(drm_file.GetPlatformFile());
      if (!version) {
        continue;
      }
      std::string version_name(
          version->name,
          base::checked_cast<std::string::size_type>(version->name_len));
      drmFreeVersion(version);
      if (base::EqualsCaseInsensitiveASCII(version_name, "vgem")) {
        continue;
      }
      drm_fd_.reset(drm_file.TakePlatformFile());
      break;
    }

    ASSERT_TRUE(drm_fd_.is_valid());
    display_ = vaGetDisplayDRM(drm_fd_.get());
    ASSERT_TRUE(vaDisplayIsValid(display_));
    int major_version;
    int minor_version;

    ASSERT_EQ(VA_STATUS_SUCCESS,
              vaInitialize(display_, &major_version, &minor_version));
  }

  void TearDown() override {
    ASSERT_EQ(VA_STATUS_SUCCESS, vaTerminate(display_));
  }

 protected:
  base::ScopedFD drm_fd_;
  VADisplay display_ = nullptr;
};

TEST_F(FakeDriverTest, VerifyQueryConfigProfiles) {
  ASSERT_GT(vaMaxNumProfiles(display_), 0);

  std::vector<VAProfile> va_profiles(
      base::checked_cast<size_t>(vaMaxNumProfiles(display_)));
  int num_va_profiles;

  const VAStatus va_res =
      vaQueryConfigProfiles(display_, va_profiles.data(), &num_va_profiles);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);

  std::set<VAProfile> unique_profiles(va_profiles.begin(), va_profiles.end());
  EXPECT_EQ(va_profiles.size(), unique_profiles.size());
}

TEST_F(FakeDriverTest, CanCreateConfig) {
  VAConfigID config_id = VA_INVALID_ID;
  const VAStatus va_res =
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr,
                     /*num_attribs=*/0, &config_id);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_NE(VA_INVALID_ID, config_id);
}

TEST_F(FakeDriverTest, QueryConfigAttributesForValidConfigID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAProfile profile = VAProfileNone;
  VAEntrypoint entrypoint = VAEntrypointProtectedContent;
  base::FixedArray<VAConfigAttrib> config_attribs(
      base::checked_cast<size_t>(vaMaxNumConfigAttributes(display_)));
  memset(config_attribs.data(), 0, config_attribs.memsize());
  int num_attribs = 0;
  const VAStatus va_res =
      vaQueryConfigAttributes(display_, config_id, &profile, &entrypoint,
                              config_attribs.data(), &num_attribs);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_EQ(VAProfileVP8Version0_3, profile);
  EXPECT_EQ(VAEntrypointVLD, entrypoint);
  EXPECT_NE(0, num_attribs);
}

TEST_F(FakeDriverTest, QueryConfigAttributesCrashesForInvalidConfigID) {
  VAProfile profile;
  VAEntrypoint entrypoint;
  base::FixedArray<VAConfigAttrib> config_attribs(
      base::checked_cast<size_t>(vaMaxNumConfigAttributes(display_)));
  memset(config_attribs.data(), 0, config_attribs.memsize());
  int num_attribs;
  EXPECT_DEATH(
      vaQueryConfigAttributes(display_, /*config_id=*/0, &profile, &entrypoint,
                              config_attribs.data(), &num_attribs);
      , "");
}

TEST_F(FakeDriverTest, CreateContextForValidConfigID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id = VA_INVALID_ID;
  const VAStatus va_res = vaCreateContext(
      display_, config_id, /*picture_width=*/1280, /*picture_height=*/720,
      /*flag=*/0, /*render_targets=*/nullptr,
      /*num_render_targets=*/0, &context_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, CreateContextCrashesForInvalidConfigID) {
  VAContextID context_id = VA_INVALID_ID;
  EXPECT_DEATH(
      vaCreateContext(display_, /*config_id=*/0, /*picture_width=*/1280,
                      /*picture_height=*/720, /*flag=*/0,
                      /*render_targets=*/nullptr,
                      /*num_render_targets=*/0, &context_id),
      "");
}

TEST_F(FakeDriverTest, QuerySurfaceAttributesForValidConfigID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  constexpr unsigned int max_num_attribs = 32;
  unsigned int num_attribs = 32;
  VASurfaceAttrib surface_attribs[max_num_attribs];
  memset(surface_attribs, 0, sizeof(surface_attribs));

  const VAStatus va_res = vaQuerySurfaceAttributes(
      display_, config_id, surface_attribs, &num_attribs);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_NE(0u, num_attribs);
}

TEST_F(FakeDriverTest, QuerySurfaceAttributesCrashesForInvalidConfigID) {
  unsigned int num_attribs = 0;
  EXPECT_DEATH(
      vaQuerySurfaceAttributes(display_, /*config_id=*/0,
                               /*surface_attribs=*/nullptr, &num_attribs),
      "");
}

TEST_F(FakeDriverTest, DestroyConfigForValidConfigID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  const VAStatus va_res = vaDestroyConfig(display_, config_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, DestroyConfigCrashesForInvalidConfigID) {
  EXPECT_DEATH(vaDestroyConfig(display_, /*config_id=*/0), "");
}

TEST_F(FakeDriverTest, CanCreateSurfaces) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  const VAStatus va_res =
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    EXPECT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }
}

TEST_F(FakeDriverTest, CanCreateContextForValidSurfaceAndConfigIDs) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id;
  const VAStatus va_res = vaCreateContext(
      display_, config_id, /*picture_width=*/1280, /*picture_height=*/720,
      /*flag=*/0, surfaces, kNumSurfaces, &context_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, CreateContextCrashesForInvalidSurfaceID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id;
  VASurfaceID invalid_id[1] = {0};
  EXPECT_DEATH(vaCreateContext(display_, config_id, /*picture_width=*/1280,
                               /*picture_height=*/720, /*flag=*/0, invalid_id,
                               /*num_render_targets=*/1, &context_id),
               "");
}

TEST_F(FakeDriverTest, CanBeginPictureForValidSurfaceID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id = VA_INVALID_ID;
  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720, /*flag=*/0, surfaces,
                            kNumSurfaces, &context_id));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    const VAStatus va_res = vaBeginPicture(display_, context_id, surfaces[i]);
    EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
  }
}

TEST_F(FakeDriverTest, BeginPictureCrashesForInvalidSurfaceID) {
  VAConfigID config_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));

  VAContextID context_id = VA_INVALID_ID;
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateContext(display_, config_id, /*picture_width=*/1280,
                      /*picture_height=*/720, /*flag=*/0, /*surfaces=*/nullptr,
                      /*num_surfaces=*/0, &context_id));

  EXPECT_DEATH(vaBeginPicture(display_, context_id, /*render_target=*/0), "");
}

TEST_F(FakeDriverTest, CanSyncSurfaceForValidSurfaceID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    const VAStatus va_res = vaSyncSurface(display_, surfaces[i]);
    EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
  }
}

TEST_F(FakeDriverTest, SyncSurfaceCrashesForInvalidSurfaceID) {
  EXPECT_DEATH(vaSyncSurface(display_, /*render_target=*/0), "");
}

TEST_F(FakeDriverTest, CanGetImageForValidSurfaceID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  VAImageFormat image_format_nv12{.fourcc = VA_FOURCC_NV12,
                                  .byte_order = VA_LSB_FIRST,
                                  .bits_per_pixel = 12};

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    VAImage img;
    ASSERT_EQ(VA_STATUS_SUCCESS,
              vaCreateImage(display_, &image_format_nv12, /*width=*/1280,
                            /*height=*/720, &img));
    ASSERT_NE(img.image_id, VA_INVALID_ID);
    ASSERT_EQ(
        VA_STATUS_SUCCESS,
        vaGetImage(display_, surfaces[i], /*x=*/0, /*y=*/0,
                   /*width=*/1280, /*height=*/720, /*image=*/img.image_id));
  }
}

TEST_F(FakeDriverTest, GetImageCrashesForInvalidSurfaceID) {
  // TODO(b/258275488): Provide a valid Image ID once that functionality is
  // implemented.
  EXPECT_DEATH(vaGetImage(display_, /*surface=*/0, /*x=*/0, /*y=*/0,
                          /*width=*/1280, /*height=*/720, /*image=*/0),
               "");
}

TEST_F(FakeDriverTest, CanPutImageForValidSurfaceID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    // TODO(b/258275488): Provide a valid Image ID once that functionality is
    // implemented.
    const VAStatus va_res =
        vaPutImage(display_, surfaces[i], /*image=*/0, /*src_x=*/0, /*src_y=*/0,
                   /*src_width=*/1280, /*src_height=*/720, /*dest_x=*/0,
                   /*dest_y=*/0, /*dest_width=*/1280, /*dest_height=*/720);
    EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
  }
}

TEST_F(FakeDriverTest, PutImageCrashesForInvalidSurfaceID) {
  // TODO(b/258275488): Provide a valid Image ID once that functionality is
  // implemented.
  EXPECT_DEATH(vaPutImage(display_, /*surface=*/0, /*image=*/0, /*src_x=*/0,
                          /*src_y=*/0,
                          /*src_width=*/1280, /*src_height=*/720,
                          /*dest_x=*/0, /*dest_y=*/0,
                          /*dest_width=*/1280, /*dest_height=*/720),
               "");
}

TEST_F(FakeDriverTest, CanDeriveImageForValidSurfaceID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    VAImage image;
    const VAStatus va_res = vaDeriveImage(display_, surfaces[i], &image);
    EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
  }
}

TEST_F(FakeDriverTest, DeriveImageCrashesForInvalidSurfaceID) {
  VAImage image;
  EXPECT_DEATH(vaDeriveImage(display_, /*surface=*/0, &image), "");
}

TEST_F(FakeDriverTest, CanDestroySurfacesForValidSurfaceIDs) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  const VAStatus va_res = vaDestroySurfaces(display_, surfaces, kNumSurfaces);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, DestroySurfacesCrashesForInvalidSurfaceIDs) {
  VASurfaceID invalid_id[1] = {0};
  EXPECT_DEATH(vaDestroySurfaces(display_, invalid_id, /*num_surfaces=*/1), "");
}

TEST_F(FakeDriverTest, CanCreateContext) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420, /*width=*/1280,
                       /*height=*/720, surfaces, kNumSurfaces,
                       /*surface_attribs=*/nullptr, /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;

  const VAStatus va_res = vaCreateContext(
      display_, config_id, /*picture_width=*/1280, /*picture_height=*/720,
      /*flag=*/0, surfaces, kNumSurfaces, &context_id);
  ASSERT_EQ(VA_STATUS_SUCCESS, va_res);
  EXPECT_NE(VA_INVALID_ID, context_id);
}

TEST_F(FakeDriverTest, CanCreateBufferForValidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720,
                            /*flag=*/0, surfaces, kNumSurfaces, &context_id));
  ASSERT_NE(VA_INVALID_ID, context_id);

  VABufferID buf_id = VA_INVALID_ID;
  VABufferType buf_type = VASliceDataBufferType;
  const VAStatus va_res = vaCreateBuffer(display_, context_id, buf_type,
                                         /*size=*/1024, /*num_elements=*/1,
                                         /*data=*/nullptr, &buf_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, CreateBufferCrashesForInvalidContextID) {
  VABufferID buf_id = VA_INVALID_ID;
  VABufferType buf_type = VASliceDataBufferType;
  EXPECT_DEATH(vaCreateBuffer(display_, /*context_id=*/0, buf_type,
                              /*size=*/1024, /*num_elements=*/1,
                              /*data=*/nullptr, &buf_id),
               "");
}

TEST_F(FakeDriverTest, CanBeginPictureForValidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;
  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720,
                            /*flag=*/0, surfaces, kNumSurfaces, &context_id));
  ASSERT_NE(VA_INVALID_ID, context_id);

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    const VAStatus va_res = vaBeginPicture(display_, context_id, surfaces[i]);
    EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
  }
}

TEST_F(FakeDriverTest, BeginPictureCrashesForInvalidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    EXPECT_DEATH(vaBeginPicture(display_, /*context=*/0, surfaces[i]), "");
  }
}

TEST_F(FakeDriverTest, CanRenderPictureForValidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720,
                            /*flag=*/0, surfaces, kNumSurfaces, &context_id));
  ASSERT_NE(VA_INVALID_ID, context_id);

  // TODO(b/258275488): Provide a valid Buffer ID once that functionality is
  // implemented.
  const VAStatus va_res = vaRenderPicture(
      display_, context_id, /*buf_id=*/nullptr, /*num_buffers=*/0);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, RenderPictureCrashesForInvalidContextID) {
  // TODO(b/258275488): Provide a valid Buffer ID once that functionality is
  // implemented.
  EXPECT_DEATH(vaRenderPicture(display_, /*context=*/0, /*buf_id=*/nullptr,
                               /*num_buffers=*/0),
               "");
}

TEST_F(FakeDriverTest, CanEndPictureForValidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720,
                            /*flag=*/0, surfaces, kNumSurfaces, &context_id));
  ASSERT_NE(VA_INVALID_ID, context_id);

  const VAStatus va_res = vaEndPicture(display_, context_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, EndPictureCrashesForInvalidContextID) {
  EXPECT_DEATH(vaEndPicture(display_, /*context=*/0), "");
}

TEST_F(FakeDriverTest, CanDestroyContextForValidContextID) {
  constexpr unsigned int kNumSurfaces = 32;
  VASurfaceID surfaces[kNumSurfaces];
  std::fill(surfaces, surfaces + kNumSurfaces, VA_INVALID_SURFACE);

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateSurfaces(display_, /*format=*/VA_RT_FORMAT_YUV420,
                             /*width=*/1280, /*height=*/720, surfaces,
                             kNumSurfaces, /*surface_attribs=*/nullptr,
                             /*num_attribs=*/0));

  for (unsigned int i = 0; i < kNumSurfaces; i++) {
    ASSERT_NE(VA_INVALID_SURFACE, surfaces[i]);
  }

  VAConfigID config_id = VA_INVALID_ID;

  ASSERT_EQ(
      VA_STATUS_SUCCESS,
      vaCreateConfig(display_, VAProfileVP8Version0_3, VAEntrypointVLD,
                     /*attrib_list=*/nullptr, /*num_attribs=*/0, &config_id));
  ASSERT_NE(VA_INVALID_ID, config_id);

  VAContextID context_id = VA_INVALID_ID;

  ASSERT_EQ(VA_STATUS_SUCCESS,
            vaCreateContext(display_, config_id, /*picture_width=*/1280,
                            /*picture_height=*/720,
                            /*flag=*/0, surfaces, kNumSurfaces, &context_id));
  ASSERT_NE(VA_INVALID_ID, context_id);

  const VAStatus va_res = vaDestroyContext(display_, context_id);
  EXPECT_EQ(VA_STATUS_SUCCESS, va_res);
}

TEST_F(FakeDriverTest, DestroyContextCrashesForInvalidContextID) {
  EXPECT_DEATH(vaDestroyContext(display_, /*context=*/0), "");
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // We stub out the ContextDelegate so that the driver doesn't fail assertions
  // in places that we don't care about unit testing: those places require
  // something closer to integration testing due to the number of moving parts.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  CHECK(env);
  env->SetVar("USE_NO_OP_CONTEXT_DELEGATE", "1");

  const std::string va_suffix(base::NumberToString(VA_MAJOR_VERSION + 1));
  StubPathMap paths;

  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  paths[kModuleVa_prot].push_back(std::string("libva.so.") + va_suffix);
#endif

  // InitializeStubs dlopen() VA-API libraries.
  const bool result = InitializeStubs(paths);
  if (!result) {
    LOG(ERROR) << "Failed to initialize the libva symbols";
    return EXIT_FAILURE;
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
