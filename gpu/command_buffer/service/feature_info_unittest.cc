// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/feature_info.h"

#include <stddef.h>

#include <memory>

#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_version_info.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

namespace {
const char kGLRendererStringANGLE[] = "ANGLE (some renderer)";
}  // anonymous namespace

enum MockedGLVersionKind {
  ES2_on_Version3_0,
  ES2_on_Version3_2Compatibility,
  ES3_on_Version3_0,
  ES3_on_Version3_2Compatibility,

  // Currently, nothing cares about both ES version and passthrough, so just
  // create one representative passthrough case.
  ES2_on_Version3_0_Passthrough
};

class FeatureInfoTest
    : public GpuServiceTest,
      public ::testing::WithParamInterface<MockedGLVersionKind> {
 public:
  FeatureInfoTest() = default;

  void SetupInitExpectations(const char* extensions) {
    std::string extensions_str = extensions;
    // Most of the tests' expectations currently assume the desktop
    // OpenGL compatibility profile.
    switch (GetParam()) {
      case ES2_on_Version3_0:
      case ES2_on_Version3_0_Passthrough:
      case ES3_on_Version3_0:
        SetupInitExpectationsWithGLVersion(extensions_str.c_str(), "", "3.0");
        break;
      case ES2_on_Version3_2Compatibility:
      case ES3_on_Version3_2Compatibility:
        if (extensions_str.find("GL_ARB_compatibility") == std::string::npos) {
          extensions_str += " GL_ARB_compatibility";
        }
        SetupInitExpectationsWithGLVersion(extensions_str.c_str(), "", "3.2");
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  ContextType GetContextType() {
    switch (GetParam()) {
      case ES2_on_Version3_0:
      case ES2_on_Version3_0_Passthrough:
      case ES2_on_Version3_2Compatibility:
        return CONTEXT_TYPE_OPENGLES2;
      case ES3_on_Version3_0:
      case ES3_on_Version3_2Compatibility:
        return CONTEXT_TYPE_OPENGLES3;
      default:
        NOTREACHED();
        return CONTEXT_TYPE_OPENGLES2;
    }
  }

  bool IsPassthroughCmdDecoder() {
    switch (GetParam()) {
      case ES2_on_Version3_0_Passthrough:
        return true;
      case ES2_on_Version3_0:
      case ES2_on_Version3_2Compatibility:
      case ES3_on_Version3_0:
      case ES3_on_Version3_2Compatibility:
        return false;
      default:
        NOTREACHED();
        return false;
    }
  }

  void SetupInitExpectationsWithGLVersion(
      const char* extensions, const char* renderer, const char* version) {
    GpuServiceTest::SetUpWithGLVersion(version, extensions);
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), extensions, renderer, version, GetContextType());
    info_ = new FeatureInfo();
    info_->Initialize(GetContextType(), IsPassthroughCmdDecoder(),
                      DisallowedFeatures());
  }

  void SetupInitExpectationsWithGLVersionAndDisallowedFeatures(
      const char* extensions,
      const char* renderer,
      const char* version,
      const DisallowedFeatures& disallowed_features) {
    GpuServiceTest::SetUpWithGLVersion(version, extensions);
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), extensions, renderer, version, GetContextType());
    info_ = new FeatureInfo();
    info_->Initialize(GetContextType(), IsPassthroughCmdDecoder(),
                      disallowed_features);
  }

  void SetupWithWorkarounds(const gpu::GpuDriverBugWorkarounds& workarounds) {
    GpuServiceTest::SetUp();
    info_ = new FeatureInfo(workarounds, GpuFeatureInfo());
  }

  void SetupInitExpectationsWithWorkarounds(
      const char* extensions,
      const gpu::GpuDriverBugWorkarounds& workarounds) {
    GpuServiceTest::SetUpWithGLVersion("2.0", extensions);
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), extensions, "", "", GetContextType());
    info_ = new FeatureInfo(workarounds, GpuFeatureInfo());
    info_->Initialize(GetContextType(), IsPassthroughCmdDecoder(),
                      DisallowedFeatures());
  }

  void SetupWithoutInit() {
    GpuServiceTest::SetUp();
    info_ = new FeatureInfo();
  }

 protected:
  void SetUp() override {
    // Do nothing here, since we are using the explicit Setup*() functions.
  }

  void TearDown() override {
    info_ = nullptr;
    GpuServiceTest::TearDown();
  }

  scoped_refptr<FeatureInfo> info_;
};

static const MockedGLVersionKind kGLVersionKinds[] = {
  ES2_on_Version3_0,
  ES2_on_Version3_2Compatibility,
  ES3_on_Version3_0,
  ES3_on_Version3_2Compatibility
};

INSTANTIATE_TEST_SUITE_P(Service,
                         FeatureInfoTest,
                         ::testing::ValuesIn(kGLVersionKinds));

TEST_P(FeatureInfoTest, Basic) {
  SetupWithoutInit();
  // Test it starts off uninitialized.
  EXPECT_FALSE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_FALSE(info_->feature_flags().multisampled_render_to_texture);
  EXPECT_FALSE(info_->feature_flags(
      ).use_img_for_multisampled_render_to_texture);
  EXPECT_FALSE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_FALSE(info_->feature_flags().oes_egl_image_external);
  EXPECT_FALSE(info_->feature_flags().nv_egl_stream_consumer_external);
  EXPECT_FALSE(info_->feature_flags().oes_depth24);
  EXPECT_FALSE(info_->feature_flags().packed_depth24_stencil8);
  EXPECT_FALSE(info_->feature_flags().angle_translated_shader_source);
  EXPECT_FALSE(info_->feature_flags().angle_pack_reverse_row_order);
  EXPECT_FALSE(info_->feature_flags().arb_texture_rectangle);
  EXPECT_FALSE(info_->feature_flags().angle_instanced_arrays);
  EXPECT_FALSE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags().native_vertex_array_object);
  EXPECT_FALSE(info_->feature_flags().map_buffer_range);
  EXPECT_FALSE(info_->feature_flags().use_async_readpixels);
  EXPECT_FALSE(info_->feature_flags().ext_draw_buffers);
  EXPECT_FALSE(info_->feature_flags().nv_draw_buffers);
  EXPECT_FALSE(info_->feature_flags().ext_discard_framebuffer);
  EXPECT_FALSE(info_->feature_flags().angle_depth_texture);
  EXPECT_FALSE(info_->feature_flags().ext_read_format_bgra);

#define GPU_OP(type, name) EXPECT_FALSE(info_->workarounds().name);
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  EXPECT_EQ(0, info_->workarounds().max_texture_size);
  EXPECT_FALSE(info_->workarounds().gl_clear_broken);
}

TEST_P(FeatureInfoTest, InitializeNoExtensions) {
  SetupInitExpectations("");
  // Check default extensions are there
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_resource_safe"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_strict_attribs"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_translated_shader_source"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_trace_marker"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_unpack_subimage"));

  bool expect_ext_srgb = false;
  switch (GetParam()) {
    case ES2_on_Version3_0:
    case ES3_on_Version3_0:
    case ES3_on_Version3_2Compatibility:
      break;
    case ES2_on_Version3_2Compatibility:
      // sRGB features are available as core GL 3.2.
      expect_ext_srgb = true;
      break;
    default:
      NOTREACHED();
      break;
  }
  // Note that because GL_EXT_sRGB is a substring of GL_EXT_sRGB_write_control,
  // which is not part of the ES3 core, we have to be careful to search for
  // "GL_EXT_sRGB ", and append a space to the end of the extension string.
  if (expect_ext_srgb) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_sRGB"));
    EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_SRGB_EXT));
    EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_SRGB_ALPHA_EXT));
    EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_EXT));
    EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_ALPHA_EXT));
    EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
        GL_SRGB8_ALPHA8_EXT));
    EXPECT_TRUE(info_->validators()->framebuffer_attachment_parameter.IsValid(
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT));
  } else {
    EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_EXT_sRGB"));
    EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_SRGB_EXT));
    EXPECT_FALSE(info_->validators()->texture_format.IsValid(
        GL_SRGB_ALPHA_EXT));
    EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_EXT));
    EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_ALPHA_EXT));
    EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
        GL_SRGB8_ALPHA8_EXT));
    EXPECT_FALSE(info_->validators()->framebuffer_attachment_parameter.IsValid(
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT));
  }

  // Check a couple of random extensions that should not be there.
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_npot"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_ANGLE_texture_compression_dxt1"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_ANGLE_texture_compression_dxt3"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_ANGLE_texture_compression_dxt5"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_ANGLE_texture_usage"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_OES_compressed_ETC1_RGB8_texture"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_AMD_compressed_ATC_texture"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_IMG_texture_compression_pvrtc"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_EXT_texture_compression_s3tc_srgb"));
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_ETC1_RGB8_OES));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGB_AMD));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGBA_EXPLICIT_ALPHA_AMD));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG));
  EXPECT_FALSE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(info_->validators()->framebuffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->framebuffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_FALSE(info_->validators()->render_buffer_parameter.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_RGBA32F));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_RGB32F));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
  EXPECT_FALSE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_USAGE_ANGLE));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT16));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT32_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH24_STENCIL8_OES));
  EXPECT_FALSE(info_->validators()->equation.IsValid(GL_MIN_EXT));
  EXPECT_FALSE(info_->validators()->equation.IsValid(GL_MAX_EXT));
  EXPECT_FALSE(info_->feature_flags().chromium_sync_query);
  EXPECT_FALSE(info_->feature_flags().chromium_image_ycbcr_422);
}

TEST_P(FeatureInfoTest, InitializeWithANGLE) {
  SetupInitExpectationsWithGLVersion("", kGLRendererStringANGLE, "");
  EXPECT_TRUE(info_->gl_version_info().is_angle);
}

TEST_P(FeatureInfoTest, InitializeWithANGLED3D9Ex) {
  SetupInitExpectationsWithGLVersion("", "ANGLE (foo bar Direct3D9Ex baz)", "");
  EXPECT_TRUE(info_->gl_version_info().is_angle);
  EXPECT_TRUE(info_->gl_version_info().is_d3d);
}

TEST_P(FeatureInfoTest, InitializeWithANGLED3D11) {
  SetupInitExpectationsWithGLVersion("", "ANGLE (foo bar Direct3D11 baz)", "");
  EXPECT_TRUE(info_->gl_version_info().is_angle);
  EXPECT_TRUE(info_->gl_version_info().is_d3d);
}

TEST_P(FeatureInfoTest, InitializeWithANGLEOpenGL) {
  SetupInitExpectationsWithGLVersion("", "ANGLE (foo bar OpenGL baz)", "");
  EXPECT_TRUE(info_->gl_version_info().is_angle);
  EXPECT_FALSE(info_->gl_version_info().is_d3d);
}

TEST_P(FeatureInfoTest, InitializeNPOTExtensionGLES) {
  SetupInitExpectations("GL_OES_texture_npot");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_npot"));
  EXPECT_TRUE(info_->feature_flags().npot_ok);
}

TEST_P(FeatureInfoTest, InitializeNPOTExtensionGL) {
  SetupInitExpectations("GL_ARB_texture_non_power_of_two");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_npot"));
  EXPECT_TRUE(info_->feature_flags().npot_ok);
}

TEST_P(FeatureInfoTest, InitializeDXTExtensionGLES2) {
  SetupInitExpectations("GL_ANGLE_texture_compression_dxt1");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_texture_compression_dxt1"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_P(FeatureInfoTest, InitializeDXTExtensionGL) {
  SetupInitExpectations("GL_EXT_texture_compression_s3tc");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_texture_compression_dxt1"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_texture_compression_dxt3"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_texture_compression_dxt5"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_compression_s3tc_srgb) {
  SetupInitExpectationsWithGLVersion("GL_NV_sRGB_formats", "",
                                     "OpenGL ES 2.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_EXT_texture_compression_s3tc_srgb"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_compression_s3tc_srgbGL) {
  SetupInitExpectations("GL_EXT_texture_sRGB GL_EXT_texture_compression_s3tc");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_EXT_texture_compression_s3tc_srgb"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT));
}

TEST_P(FeatureInfoTest, InitializeANGLE_compressed_texture_etc) {
  SetupInitExpectationsWithGLVersion("", "",
                                     "OpenGL ES 3.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_ANGLE_compressed_texture_etc"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_R11_EAC));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SIGNED_R11_EAC));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB8_ETC2));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB8_ETC2));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RG11_EAC));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SIGNED_RG11_EAC));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA8_ETC2_EAC));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC));

  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_R11_EAC));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_SIGNED_R11_EAC));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_RGB8_ETC2));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_SRGB8_ETC2));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_RG11_EAC));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_SIGNED_RG11_EAC));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_RGBA8_ETC2_EAC));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888GLES2) {
  SetupInitExpectationsWithGLVersion("GL_EXT_texture_format_BGRA8888", "",
                                     "OpenGL ES 2.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));

  // On GL ES, render buffer and read pixels functionality is unrelated to
  // GL_EXT_texture_format_BGRA8888, make sure we don't enable it.
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_BGRA8_EXT));
  EXPECT_FALSE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_FALSE(info_->validators()->read_pixel_format.IsValid(GL_BGRA8_EXT));
  EXPECT_FALSE(info_->feature_flags().ext_read_format_bgra);
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888GL) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL 2.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888Apple) {
  SetupInitExpectationsWithGLVersion("GL_APPLE_texture_format_BGRA8888", "",
                                     "OpenGL ES 2.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));

  // On GL ES, render buffer and read pixels functionality is unrelated to
  // GL_APPLE_texture_format_BGRA8888, make sure we don't enable it.
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_BGRA8_EXT));
  EXPECT_FALSE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_FALSE(info_->validators()->read_pixel_format.IsValid(GL_BGRA8_EXT));
  EXPECT_FALSE(info_->feature_flags().ext_read_format_bgra);
}

TEST_P(FeatureInfoTest, InitializeGLES_no_EXT_texture_format_BGRA8888GL) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 2.0");
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_BGRA_EXT));
  EXPECT_FALSE(
      info_->validators()->texture_internal_format.IsValid(GL_BGRA_EXT));
}

TEST_P(FeatureInfoTest, InitializeGLES2EXT_read_format_bgra) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_read_format_bgra", "", "OpenGL ES 2.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_read_format_bgra"));
  EXPECT_TRUE(info_->feature_flags().ext_read_format_bgra);
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));

  // On GL ES, texture and render buffer functionality is unrelated to
  // GL_EXT_read_format_bgra, make sure we don't enable it.
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_BGRA8_EXT));
  EXPECT_FALSE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_BGRA_EXT));
  EXPECT_FALSE(
      info_->validators()->texture_internal_format.IsValid(GL_BGRA_EXT));
}

TEST_P(FeatureInfoTest, InitializeGLEXT_read_format_bgra) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL 2.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_read_format_bgra"));
  EXPECT_TRUE(info_->feature_flags().ext_read_format_bgra);
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
}

TEST_P(FeatureInfoTest, InitializeGLES_no_EXT_read_format_bgra) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 2.0");
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_read_format_bgra"));
  EXPECT_FALSE(info_->feature_flags().ext_read_format_bgra);
  EXPECT_FALSE(info_->validators()->read_pixel_format.IsValid(GL_BGRA_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_sRGB) {
  SetupInitExpectations("GL_EXT_sRGB GL_OES_rgb8_rgba8");

  if (GetContextType() == CONTEXT_TYPE_OPENGLES3) {
    EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_EXT_sRGB"));
    EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_SRGB_EXT));
    EXPECT_FALSE(
        info_->validators()->texture_format.IsValid(GL_SRGB_ALPHA_EXT));
    EXPECT_FALSE(
        info_->validators()->texture_internal_format.IsValid(GL_SRGB_EXT));
    EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_ALPHA_EXT));
    EXPECT_FALSE(
        info_->validators()->render_buffer_format.IsValid(GL_SRGB8_ALPHA8_EXT));
    EXPECT_FALSE(info_->validators()->framebuffer_attachment_parameter.IsValid(
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT));
  } else {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_sRGB"));
    EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_SRGB_EXT));
    EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_SRGB_ALPHA_EXT));
    EXPECT_TRUE(
        info_->validators()->texture_internal_format.IsValid(GL_SRGB_EXT));
    EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
        GL_SRGB_ALPHA_EXT));
    EXPECT_TRUE(
        info_->validators()->render_buffer_format.IsValid(GL_SRGB8_ALPHA8_EXT));
    EXPECT_TRUE(info_->validators()->framebuffer_attachment_parameter.IsValid(
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT));
  }
}

TEST_P(FeatureInfoTest, InitializeGLES2EXT_texture_storage) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_storage", "", "OpenGL ES 2.0");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_IMMUTABLE_FORMAT_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_BGRA8_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA16F_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_storage) {
  SetupInitExpectations("GL_EXT_texture_storage");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_IMMUTABLE_FORMAT_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_BGRA8_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA32F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE16F_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA16F_EXT));
}

TEST_P(FeatureInfoTest, InitializeARB_texture_storage) {
  SetupInitExpectations("GL_ARB_texture_storage");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_IMMUTABLE_FORMAT_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_storage_BGRA) {
  SetupInitExpectationsWithGLVersion("GL_EXT_texture_storage", "",
                                     "OpenGL 2.0");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_BGRA8_EXT));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

TEST_P(FeatureInfoTest, InitializeARB_texture_storage_BGRA) {
  SetupInitExpectationsWithGLVersion("GL_ARB_texture_storage", "",
                                     "OpenGL 2.0");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_BGRA8_EXT));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_storage_BGRA8888) {
  SetupInitExpectations(
      "GL_EXT_texture_storage GL_EXT_texture_format_BGRA8888");
  EXPECT_TRUE(info_->feature_flags().ext_texture_storage);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_BGRA8_EXT));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_storage_float) {
  SetupInitExpectations("GL_EXT_texture_storage GL_OES_texture_float");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA32F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB32F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA32F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE32F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA32F_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_storage_half_float) {
  SetupInitExpectations("GL_EXT_texture_storage GL_OES_texture_half_float");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGBA16F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_RGB16F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_ALPHA16F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE16F_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_LUMINANCE_ALPHA16F_EXT));
}

// Check that desktop (unlike ES, handled below) always supports BGRA render
// buffers.
TEST_P(FeatureInfoTest, InitializeGL_renderbuffer_format_BGRA8888) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL 2.0");

  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_renderbuffer_format_BGRA8888"));
  EXPECT_TRUE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_BGRA8_EXT));
}

// Check how to handle ES, texture_storage and BGRA combination; 10 tests.

// 1- ES2 + GL_EXT_texture_storage -> GL_EXT_texture_storage (and no
// GL_EXT_texture_format_BGRA8888 - we don't claim to handle GL_BGRA8 in
// glTexStorage2DEXT)
TEST_P(FeatureInfoTest, InitializeGLES2_texture_storage) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_storage", "", "OpenGL ES 2.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 2- ES2 + GL_EXT_texture_storage + (GL_EXT_texture_format_BGRA8888 or
// GL_APPLE_texture_format_bgra8888)
TEST_P(FeatureInfoTest, InitializeGLES2_texture_storage_BGRA) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_storage GL_EXT_texture_format_BGRA8888",
      "",
      "OpenGL ES 2.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 3- ES2 + GL_EXT_texture_format_BGRA8888 or GL_APPLE_texture_format_bgra8888
TEST_P(FeatureInfoTest, InitializeGLES2_texture_format_BGRA) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_format_BGRA8888", "", "OpenGL ES 2.0");
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 4- ES2 (neither GL_EXT_texture_storage nor GL_EXT_texture_format_BGRA8888) ->
// nothing
TEST_P(FeatureInfoTest, InitializeGLES2_neither_texture_storage_nor_BGRA) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 2.0");
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 5- ES3 + GL_EXT_texture_format_BGRA8888
// If creating a GLES2 context, expose GL_EXT_texture_format_BGRA8888
// If creating a GLES3 context, expose GL_EXT_texture_storage
// (we can't expose both at the same time because we fail the GL_BGRA8
// requirement)
TEST_P(FeatureInfoTest, InitializeGLES3_texture_storage_EXT_BGRA) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_format_BGRA8888", "", "OpenGL ES 3.0");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES3) {
    EXPECT_TRUE(
        gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
    EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                   "GL_EXT_texture_format_BGRA8888"));
  } else {
    EXPECT_FALSE(
        gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_texture_format_BGRA8888"));
  }
}

// 6- ES3 + GL_APPLE_texture_format_bgra8888 -> GL_EXT_texture_storage +
// GL_EXT_texture_format_BGRA8888 (driver promises to handle GL_BGRA8 by
// exposing GL_APPLE_texture_format_bgra8888)
TEST_P(FeatureInfoTest, InitializeGLES3_texture_storage_APPLE_BGRA) {
  SetupInitExpectationsWithGLVersion(
      "GL_APPLE_texture_format_BGRA8888", "", "OpenGL ES 3.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 7- ES3 + GL_EXT_texture_storage + GL_EXT_texture_format_BGRA8888 ->
// GL_EXT_texture_storage + GL_EXT_texture_format_BGRA8888  (driver promises to
// handle GL_BGRA8 by exposing GL_EXT_texture_storage)
TEST_P(FeatureInfoTest, InitializeGLES3_EXT_texture_storage_EXT_BGRA) {
  SetupInitExpectationsWithGLVersion(
      "GL_EXT_texture_storage GL_EXT_texture_format_BGRA8888",
      "",
      "OpenGL ES 3.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 8- ES3 + none of the above -> GL_EXT_texture_storage (and no
// GL_EXT_texture_format_BGRA8888 - we don't claim to handle GL_BGRA8)
TEST_P(FeatureInfoTest, InitializeGLES3_texture_storage) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 3.0");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_texture_storage"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_texture_format_BGRA8888"));
}

// 9- ANGLE will add the GL_CHROMIUM_renderbuffer_format_BGRA8888 extension and
// the GL_BGRA8_EXT render buffer format.
TEST_P(FeatureInfoTest, InitializeWithANGLE_BGRA8) {
  SetupInitExpectationsWithGLVersion("", kGLRendererStringANGLE, "");
  EXPECT_TRUE(info_->gl_version_info().is_angle);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_renderbuffer_format_BGRA8888"));
  EXPECT_TRUE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_BGRA8_EXT));
}

// 10- vanilla opengl es means no GL_CHROMIUM_renderbuffer_format_BGRA8888
TEST_P(FeatureInfoTest,
       InitializeGLES2_no_CHROMIUM_renderbuffer_format_BGRA8888) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 2.0");
  EXPECT_FALSE(info_->feature_flags().ext_render_buffer_format_bgra8888);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_CHROMIUM_renderbuffer_format_BGRA8888"));
}

TEST_P(FeatureInfoTest, InitializeARB_texture_float) {
  DisallowedFeatures disallowed_features;
  disallowed_features.chromium_color_buffer_float_rgb = true;
  disallowed_features.chromium_color_buffer_float_rgba = true;
  SetupInitExpectationsWithGLVersionAndDisallowedFeatures(
      "GL_ARB_texture_float", "", "3.0", disallowed_features);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_CHROMIUM_color_buffer_float_rgb"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_CHROMIUM_color_buffer_float_rgba"));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_RGBA32F));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_RGB32F));

  info_->EnableCHROMIUMColorBufferFloatRGBA();
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_CHROMIUM_color_buffer_float_rgb"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_color_buffer_float_rgba"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_RGBA32F));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_RGB32F));

  info_->EnableCHROMIUMColorBufferFloatRGB();
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_color_buffer_float_rgb"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_color_buffer_float_rgba"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_RGBA32F));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_RGB32F));
}

TEST_P(FeatureInfoTest, Initialize_texture_floatGLES3) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 3.0");
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_float_linear"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_OES_texture_half_float_linear"));
}

TEST_P(FeatureInfoTest, Initialize_sRGBGLES3) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 3.0");
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_EXT_sRGB"));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(
      GL_SRGB_EXT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(
      GL_SRGB_ALPHA_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_SRGB_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_SRGB_ALPHA_EXT));
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_SRGB8_ALPHA8_EXT));
  EXPECT_FALSE(info_->validators()->framebuffer_attachment_parameter.IsValid(
      GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT));
}

TEST_P(FeatureInfoTest, InitializeOES_texture_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_float");
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_float_linear"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_OES_texture_half_float_linear"));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_texture_float_linearGLES2) {
  SetupInitExpectations("GL_OES_texture_float GL_OES_texture_float_linear");
  EXPECT_TRUE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_float_linear"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_OES_texture_half_float_linear"));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_texture_half_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_half_float");
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_float_linear"));
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_OES_texture_half_float_linear"));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_texture_half_float_linearGLES2) {
  SetupInitExpectations(
      "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_TRUE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(), "GL_OES_texture_float"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_half_float"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_OES_texture_float_linear"));
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_OES_texture_half_float_linear"));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_P(FeatureInfoTest, InitializeEXT_framebuffer_multisample) {
  SetupInitExpectations(
      "GL_EXT_framebuffer_blit GL_EXT_framebuffer_multisample");
  EXPECT_TRUE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_TRUE(
      info_->validators()->framebuffer_target.IsValid(GL_READ_FRAMEBUFFER_EXT));
  EXPECT_TRUE(
      info_->validators()->framebuffer_target.IsValid(GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_TRUE(
      info_->validators()->g_l_state.IsValid(GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
}

TEST_P(FeatureInfoTest, InitializeARB_framebuffer_multisample) {
  SetupInitExpectations("GL_ARB_framebuffer_object");
  EXPECT_TRUE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_TRUE(info_->validators()->framebuffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->framebuffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
}

TEST_P(FeatureInfoTest, InitializeANGLE_framebuffer_multisample) {
  SetupInitExpectationsWithGLVersion(
      "GL_ANGLE_framebuffer_multisample", kGLRendererStringANGLE, "");
  EXPECT_TRUE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_TRUE(info_->validators()->framebuffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->framebuffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
}

// We don't allow ANGLE_framebuffer_multisample on non-ANGLE implementations,
// because we wouldn't be choosing the right driver entry point and because the
// extension was falsely advertised on some Android devices (crbug.com/165736).
TEST_P(FeatureInfoTest, InitializeANGLE_framebuffer_multisampleWithoutANGLE) {
  SetupInitExpectations("GL_ANGLE_framebuffer_multisample");
  EXPECT_FALSE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_FALSE(info_->validators()->framebuffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->framebuffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_FALSE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_multisampled_render_to_texture) {
  SetupInitExpectations("GL_EXT_multisampled_render_to_texture");
  EXPECT_TRUE(info_->feature_flags(
      ).multisampled_render_to_texture);
  EXPECT_FALSE(info_->feature_flags(
      ).use_img_for_multisampled_render_to_texture);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_EXT_multisampled_render_to_texture"));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->framebuffer_attachment_parameter.IsValid(
      GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT));
}

TEST_P(FeatureInfoTest, InitializeIMG_multisampled_render_to_texture) {
  SetupInitExpectations("GL_IMG_multisampled_render_to_texture");
  EXPECT_TRUE(info_->feature_flags(
      ).multisampled_render_to_texture);
  EXPECT_TRUE(info_->feature_flags(
      ).use_img_for_multisampled_render_to_texture);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_EXT_multisampled_render_to_texture"));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->framebuffer_attachment_parameter.IsValid(
      GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_filter_anisotropic) {
  SetupInitExpectations("GL_EXT_texture_filter_anisotropic");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_EXT_texture_filter_anisotropic"));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_ARB_depth_texture) {
  SetupInitExpectations("GL_ARB_depth_texture");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_P(FeatureInfoTest, InitializeOES_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_depth_texture");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_P(FeatureInfoTest, InitializeANGLE_depth_texture) {
  SetupInitExpectations("GL_ANGLE_depth_texture");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_depth_texture"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_ANGLE_depth_texture"));
  EXPECT_TRUE(info_->feature_flags().angle_depth_texture);
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT16));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT32_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH24_STENCIL8_OES));
}

TEST_P(FeatureInfoTest, InitializeEXT_packed_depth_stencil) {
  SetupInitExpectations("GL_EXT_packed_depth_stencil");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_P(FeatureInfoTest, InitializeOES_packed_depth_stencil) {
  SetupInitExpectations("GL_OES_packed_depth_stencil");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_P(FeatureInfoTest,
       InitializeOES_packed_depth_stencil_and_GL_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_packed_depth_stencil GL_ARB_depth_texture");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
}

TEST_P(FeatureInfoTest, InitializeOES_depth24) {
  SetupInitExpectations("GL_OES_depth24");
  EXPECT_TRUE(info_->feature_flags().oes_depth24);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_depth24"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
}

TEST_P(FeatureInfoTest, InitializeOES_standard_derivatives) {
  SetupInitExpectations("GL_OES_standard_derivatives");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_standard_derivatives"));
  EXPECT_TRUE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_TRUE(info_->validators()->hint_target.IsValid(
      GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_rgb8_rgba8) {
  SetupInitExpectations("GL_OES_rgb8_rgba8");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_rgb8_rgba8"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_RGB8_OES));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_RGBA8_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_EGL_image_external) {
  SetupInitExpectations("GL_OES_EGL_image_external");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_EGL_image_external"));
  EXPECT_TRUE(info_->feature_flags().oes_egl_image_external);
  EXPECT_TRUE(info_->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->get_tex_param_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_TEXTURE_BINDING_EXTERNAL_OES));
}

TEST_P(FeatureInfoTest, InitializeNV_EGL_stream_consumer_external) {
  SetupInitExpectations("GL_NV_EGL_stream_consumer_external");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_NV_EGL_stream_consumer_external"));
  EXPECT_TRUE(info_->feature_flags().nv_egl_stream_consumer_external);
  EXPECT_TRUE(info_->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->get_tex_param_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES));
  EXPECT_TRUE(
      info_->validators()->g_l_state.IsValid(GL_TEXTURE_BINDING_EXTERNAL_OES));
}

TEST_P(FeatureInfoTest, InitializeOES_compressed_ETC1_RGB8_texture) {
  SetupInitExpectations("GL_OES_compressed_ETC1_RGB8_texture");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_OES_compressed_ETC1_RGB8_texture"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_ETC1_RGB8_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_ETC1_RGB8_OES));
}

TEST_P(FeatureInfoTest, InitializeAMD_compressed_ATC_texture) {
  SetupInitExpectations("GL_AMD_compressed_ATC_texture");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_AMD_compressed_ATC_texture"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGB_AMD));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGBA_EXPLICIT_ALPHA_AMD));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD));
}

TEST_P(FeatureInfoTest, InitializeIMG_texture_compression_pvrtc) {
  SetupInitExpectations("GL_IMG_texture_compression_pvrtc");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_IMG_texture_compression_pvrtc"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG));
}

TEST_P(FeatureInfoTest, InitializeEXT_occlusion_query_boolean) {
  SetupInitExpectations("GL_EXT_occlusion_query_boolean");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeARB_occlusion_query) {
  SetupInitExpectations("GL_ARB_occlusion_query");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_TRUE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeARB_occlusion_query2) {
  SetupInitExpectations("GL_ARB_occlusion_query2 GL_ARB_occlusion_query2");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_TRUE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeGLES3_occlusion_query_boolean) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 3.0");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeGL33_occlusion_query2) {
  SetupInitExpectationsWithGLVersion("", "", "3.3");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_TRUE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeGL43_occlusion_query2) {
  SetupInitExpectationsWithGLVersion("", "", "4.3");
  if (GetContextType() == CONTEXT_TYPE_OPENGLES2) {
    EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                  "GL_EXT_occlusion_query_boolean"));
  }
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
}

TEST_P(FeatureInfoTest, InitializeOES_vertex_array_object) {
  SetupInitExpectations("GL_OES_vertex_array_object");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_P(FeatureInfoTest, InitializeARB_vertex_array_object) {
  SetupInitExpectations("GL_ARB_vertex_array_object");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_P(FeatureInfoTest, InitializeAPPLE_vertex_array_object) {
  SetupInitExpectations("GL_APPLE_vertex_array_object");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_P(FeatureInfoTest, InitializeNo_vertex_array_object) {
  SetupInitExpectations("");
  // Even if the native extensions are not available the implementation
  // may still emulate the GL_OES_vertex_array_object functionality. In this
  // scenario native_vertex_array_object must be false.
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_vertex_array_object"));
  EXPECT_FALSE(info_->feature_flags().native_vertex_array_object);
}

TEST_P(FeatureInfoTest, InitializeOES_element_index_uint) {
  SetupInitExpectations("GL_OES_element_index_uint");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_OES_element_index_uint"));
  EXPECT_TRUE(info_->validators()->index_type.IsValid(GL_UNSIGNED_INT));
}

TEST_P(FeatureInfoTest, InitializeEXT_blend_minmax) {
  SetupInitExpectations("GL_EXT_blend_minmax");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_blend_minmax"));
  EXPECT_TRUE(info_->validators()->equation.IsValid(GL_MIN_EXT));
  EXPECT_TRUE(info_->validators()->equation.IsValid(GL_MAX_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_frag_depth) {
  SetupInitExpectations("GL_EXT_frag_depth");
  EXPECT_TRUE(info_->feature_flags().ext_frag_depth);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_EXT_frag_depth"));
}

TEST_P(FeatureInfoTest, InitializeEXT_shader_texture_lod) {
  SetupInitExpectations("GL_EXT_shader_texture_lod");
  EXPECT_TRUE(info_->feature_flags().ext_shader_texture_lod);
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_shader_texture_lod"));
}

TEST_P(FeatureInfoTest, InitializeEXT_discard_framebuffer) {
  SetupInitExpectations("GL_EXT_discard_framebuffer");
  EXPECT_TRUE(info_->feature_flags().ext_discard_framebuffer);
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_discard_framebuffer"));
}

TEST_P(FeatureInfoTest, InitializeSamplersWithARBSamplerObjects) {
  SetupInitExpectationsWithGLVersion(
      "GL_ARB_sampler_objects", "", "3.0");
  EXPECT_TRUE(info_->feature_flags().enable_samplers);
}

TEST_P(FeatureInfoTest, InitializeWithES3) {
  SetupInitExpectationsWithGLVersion("", "", "OpenGL ES 3.0");
  EXPECT_TRUE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_TRUE(info_->feature_flags().use_async_readpixels);
  EXPECT_TRUE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_TRUE(info_->feature_flags().oes_depth24);
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_GOOGLE_depth_texture"));
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_depth_texture"));
  EXPECT_FALSE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT_24_8));
  EXPECT_TRUE(info_->feature_flags().packed_depth24_stencil8);
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(), "GL_OES_depth24"));
  EXPECT_TRUE(
      info_->validators()->render_buffer_format.IsValid(GL_DEPTH_COMPONENT24));
  EXPECT_TRUE(
      info_->validators()->render_buffer_format.IsValid(GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->feature_flags().npot_ok);
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
  EXPECT_TRUE(info_->feature_flags().enable_samplers);
  EXPECT_TRUE(info_->feature_flags().map_buffer_range);
  EXPECT_TRUE(info_->feature_flags().ext_discard_framebuffer);
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_EXT_discard_framebuffer"));
  EXPECT_TRUE(info_->feature_flags().chromium_sync_query);
  EXPECT_TRUE(gl::GLFence::IsSupported());
}

TEST_P(FeatureInfoTest, InitializeWithES3AndDepthTexture) {
  SetupInitExpectationsWithGLVersion(
      "GL_ANGLE_depth_texture", "", "OpenGL ES 3.0");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_depth_texture"));
  EXPECT_TRUE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT_24_8));
  EXPECT_TRUE(info_->feature_flags().packed_depth24_stencil8);
  EXPECT_TRUE(
      info_->validators()->texture_internal_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
}

TEST_P(FeatureInfoTest, InitializeWithoutSamplers) {
  SetupInitExpectationsWithGLVersion("", "", "3.0");
  EXPECT_FALSE(info_->feature_flags().enable_samplers);
}

TEST_P(FeatureInfoTest, ParseDriverBugWorkaroundsSingle) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.exit_on_context_lost = true;
  // Workarounds should get parsed without the need for a context.
  SetupWithWorkarounds(workarounds);
  EXPECT_TRUE(info_->workarounds().exit_on_context_lost);
}

TEST_P(FeatureInfoTest, ParseDriverBugWorkaroundsMultiple) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.exit_on_context_lost = true;
  workarounds.max_texture_size = 4096;
  // Workarounds should get parsed without the need for a context.
  SetupWithWorkarounds(workarounds);
  EXPECT_TRUE(info_->workarounds().exit_on_context_lost);
  EXPECT_EQ(4096, info_->workarounds().max_texture_size);
}

TEST_P(FeatureInfoTest, InitializeWithARBSync) {
  SetupInitExpectations("GL_ARB_sync");
  EXPECT_TRUE(info_->feature_flags().chromium_sync_query);
  EXPECT_TRUE(gl::GLFence::IsSupported());
}

TEST_P(FeatureInfoTest, InitializeWithNVFence) {
  SetupInitExpectations("GL_NV_fence");
  EXPECT_TRUE(info_->feature_flags().chromium_sync_query);
  EXPECT_TRUE(gl::GLFence::IsSupported());
}

TEST_P(FeatureInfoTest, InitializeWithNVDrawBuffers) {
  SetupInitExpectationsWithGLVersion("GL_NV_draw_buffers", "", "OpenGL ES 3.0");
  bool is_es2 = GetContextType() == CONTEXT_TYPE_OPENGLES2;
  EXPECT_EQ(is_es2, info_->feature_flags().nv_draw_buffers);
  EXPECT_EQ(is_es2, info_->feature_flags().ext_draw_buffers);
}

TEST_P(FeatureInfoTest, InitializeWithPreferredEXTDrawBuffers) {
  SetupInitExpectationsWithGLVersion(
      "GL_NV_draw_buffers GL_EXT_draw_buffers", "", "OpenGL ES 3.0");
  bool is_es2 = GetContextType() == CONTEXT_TYPE_OPENGLES2;
  EXPECT_FALSE(info_->feature_flags().nv_draw_buffers);
  EXPECT_EQ(is_es2, info_->feature_flags().ext_draw_buffers);
}

TEST_P(FeatureInfoTest, BlendEquationAdvancedDisabled) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.disable_blend_equation_advanced = true;
  SetupInitExpectationsWithWorkarounds(
      "GL_KHR_blend_equation_advanced_coherent GL_KHR_blend_equation_advanced",
      workarounds);
  EXPECT_FALSE(info_->feature_flags().blend_equation_advanced);
  EXPECT_FALSE(info_->feature_flags().blend_equation_advanced_coherent);
}

TEST_P(FeatureInfoTest, InitializeCHROMIUM_path_rendering) {
  SetupInitExpectationsWithGLVersion(
      "GL_ARB_compatibility GL_NV_path_rendering GL_EXT_direct_state_access "
      "GL_NV_framebuffer_mixed_samples",
      "", "4.3");
  EXPECT_TRUE(info_->feature_flags().chromium_path_rendering);
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_path_rendering"));
}

TEST_P(FeatureInfoTest, InitializeCHROMIUM_path_rendering2) {
  SetupInitExpectationsWithGLVersion(
      "GL_NV_path_rendering GL_NV_framebuffer_mixed_samples", "",
      "OpenGL ES 3.1");
  EXPECT_TRUE(info_->feature_flags().chromium_path_rendering);
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_path_rendering"));
}

TEST_P(FeatureInfoTest, InitializeNoCHROMIUM_path_rendering) {
  SetupInitExpectationsWithGLVersion("GL_ARB_compatibility", "", "4.3");
  EXPECT_FALSE(info_->feature_flags().chromium_path_rendering);
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_path_rendering"));
}

TEST_P(FeatureInfoTest, InitializeNoCHROMIUM_path_rendering2) {
  SetupInitExpectationsWithGLVersion(
      "GL_ARB_compatibility GL_NV_path_rendering", "", "4.3");
  EXPECT_FALSE(info_->feature_flags().chromium_path_rendering);
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_path_rendering"));
}

TEST_P(FeatureInfoTest, InitializeNoCHROMIUM_path_rendering3) {
  // Missing framebuffer mixed samples.
  SetupInitExpectationsWithGLVersion("GL_NV_path_rendering", "",
                                     "OpenGL ES 3.1");
  EXPECT_FALSE(info_->feature_flags().chromium_path_rendering);
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_CHROMIUM_path_rendering"));
}

TEST_P(FeatureInfoTest, InitializeNoKHR_blend_equation_advanced) {
  SetupInitExpectationsWithGLVersion("GL_ARB_compatibility", "", "4.3");
  EXPECT_FALSE(info_->feature_flags().blend_equation_advanced);
  EXPECT_FALSE(
      gfx::HasExtension(info_->extensions(), "GL_KHR_blend_equation_advanced"));
}

TEST_P(FeatureInfoTest, InitializeKHR_blend_equations_advanced) {
  SetupInitExpectations("GL_KHR_blend_equation_advanced");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_KHR_blend_equation_advanced"));
  EXPECT_TRUE(info_->feature_flags().blend_equation_advanced);
}

TEST_P(FeatureInfoTest, InitializeNV_blend_equations_advanced) {
  SetupInitExpectations("GL_NV_blend_equation_advanced");
  EXPECT_TRUE(
      gfx::HasExtension(info_->extensions(), "GL_KHR_blend_equation_advanced"));
  EXPECT_TRUE(info_->feature_flags().blend_equation_advanced);
}

TEST_P(FeatureInfoTest, InitializeNoKHR_blend_equation_advanced_coherent) {
  SetupInitExpectationsWithGLVersion("GL_ARB_compatibility ", "", "4.3");
  EXPECT_FALSE(info_->feature_flags().blend_equation_advanced_coherent);
  EXPECT_FALSE(gfx::HasExtension(info_->extensions(),
                                 "GL_KHR_blend_equation_advanced_coherent"));
}

TEST_P(FeatureInfoTest, InitializeKHR_blend_equations_advanced_coherent) {
  SetupInitExpectations("GL_KHR_blend_equation_advanced_coherent");
  EXPECT_TRUE(gfx::HasExtension(info_->extensions(),
                                "GL_KHR_blend_equation_advanced_coherent"));
  EXPECT_TRUE(info_->feature_flags().blend_equation_advanced);
  EXPECT_TRUE(info_->feature_flags().blend_equation_advanced_coherent);
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_rgWithFloat) {
  SetupInitExpectations(
      "GL_EXT_texture_rg GL_OES_texture_float GL_OES_texture_half_float");
  EXPECT_TRUE(info_->feature_flags().ext_texture_rg);

  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_R8_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_RG8_EXT));
}

TEST_P(FeatureInfoTest, InitializeARB_texture_rgNoFloat) {
  SetupInitExpectations("GL_ARB_texture_rg");
  EXPECT_TRUE(info_->feature_flags().ext_texture_rg);

  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(GL_RG_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_R8_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(GL_RG8_EXT));
}

TEST_P(FeatureInfoTest, InitializeEXT_texture_norm16) {
  SetupInitExpectations("GL_EXT_texture_norm16");
  EXPECT_TRUE(info_->feature_flags().ext_texture_norm16);

  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_R16_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(GL_RED_EXT));
  EXPECT_TRUE(
      info_->validators()->texture_internal_format_storage.IsValid(GL_R16_EXT));
}

TEST_P(FeatureInfoTest, InitializeCHROMIUM_unpremultiply_and_dither_copy) {
  SetupInitExpectations("");
  switch (GetParam()) {
    case ES2_on_Version3_0_Passthrough:
      EXPECT_FALSE(info_->feature_flags().unpremultiply_and_dither_copy);
      EXPECT_FALSE(gfx::HasExtension(
          info_->extensions(), "GL_CHROMIUM_unpremultiply_and_dither_copy"));
      break;
    case ES2_on_Version3_0:
    case ES2_on_Version3_2Compatibility:
    case ES3_on_Version3_0:
    case ES3_on_Version3_2Compatibility:
      EXPECT_TRUE(info_->feature_flags().unpremultiply_and_dither_copy);
      EXPECT_TRUE(gfx::HasExtension(
          info_->extensions(), "GL_CHROMIUM_unpremultiply_and_dither_copy"));
      break;
  }
}

TEST_P(FeatureInfoTest, InitializeMESAFramebufferFlipYExtensionTrue) {
  SetupInitExpectations("GL_MESA_framebuffer_flip_y");
  EXPECT_TRUE(info_->feature_flags().mesa_framebuffer_flip_y);
}

}  // namespace gles2
}  // namespace gpu
