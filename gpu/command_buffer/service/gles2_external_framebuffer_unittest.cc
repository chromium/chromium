// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_external_framebuffer.h"

#include "base/bits.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

using testing::AtLeast;

namespace gpu {
namespace {

void CreateSharedContext(const GpuPreferences& preferences,
                         const GpuDriverBugWorkarounds& workarounds,
                         scoped_refptr<gl::GLSurface>& surface,
                         scoped_refptr<gl::GLContext>& context,
                         scoped_refptr<SharedContextState>& context_state,
                         scoped_refptr<gles2::FeatureInfo>& feature_info) {
  surface =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size());
  ASSERT_TRUE(surface);
  context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(context);
  bool result = context->MakeCurrent(surface.get());
  ASSERT_TRUE(result);

  scoped_refptr<gl::GLShareGroup> share_group =
      base::MakeRefCounted<gl::GLShareGroup>();
  feature_info =
      base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
  context_state = base::MakeRefCounted<SharedContextState>(
      std::move(share_group), surface, context,
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
      GrContextType::kGL);
  context_state->InitializeSkia(GpuPreferences(), workarounds);
  context_state->InitializeGL(GpuPreferences(), feature_info);
}

using TestParamsTuple =
    testing::tuple<viz::SharedImageFormat, int, bool, bool, bool>;
class GLES2ExternalFrameBufferTest
    : public testing::TestWithParam<TestParamsTuple> {
 public:
  explicit GLES2ExternalFrameBufferTest()
      : shared_image_manager_(
            std::make_unique<SharedImageManager>(/*is_thread_safe=*/false)) {}
  ~GLES2ExternalFrameBufferTest() override {
    // |context_state_| must be destroyed on its own context.
    bool have_context =
        context_state_->MakeCurrent(surface_.get(), true /* needs_gl */);
    gles2_external_framebuffer_->Destroy(have_context);
    backing_factory_->DestroyAllSharedImages(have_context);
  }

  void SetUp() override {
    scoped_refptr<gles2::FeatureInfo> feature_info;
    GpuDriverBugWorkarounds workarounds;
    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = use_passthrough();
    CreateSharedContext(preferences, workarounds, surface_, context_,
                        context_state_, feature_info);

    backing_factory_ = std::make_unique<SharedImageFactory>(
        preferences, workarounds, GpuFeatureInfo(), context_state_.get(),
        shared_image_manager_.get(), context_state_->memory_tracker(),
        /*is_for_display_compositor=*/false);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            shared_image_manager_.get(), nullptr);

    gles2_external_framebuffer_ =
        std::make_unique<gles2::GLES2ExternalFramebuffer>(
            use_passthrough(), *feature_info,
            shared_image_representation_factory_.get());

    const bool multisampled_framebuffers_supported =
        feature_info->feature_flags().chromium_framebuffer_multisample;
    const bool rgb8_supported = feature_info->feature_flags().oes_rgb8_rgba8;
    // The only available default render buffer formats in GLES2 have very
    // little precision.  Don't enable multisampling unless 8-bit render
    // buffer formats are available--instead fall back to 8-bit textures.

    if (multisampled_framebuffers_supported && rgb8_supported) {
      glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_sample_count_);
    }
  }

  bool use_passthrough() {
    return gles2::UsePassthroughCommandDecoder(
               base::CommandLine::ForCurrentProcess()) &&
           gles2::PassthroughCommandDecoderSupported();
  }

 protected:
  std::unique_ptr<GLTextureImageRepresentationBase> ProduceGLTextureBase(
      const Mailbox& mailbox) {
    if (use_passthrough())
      return shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
    else
      return shared_image_representation_factory_->ProduceGLTexture(mailbox);
  }

  // Creates a SharedImage that can be used for reading and writing via the
  // GLES2 interface (these tests do both).
  Mailbox CreateSharedImage(const viz::SharedImageFormat& format) {
    auto mailbox = Mailbox::Generate();
    backing_factory_->CreateSharedImage(
        mailbox, format, gfx::Size(64, 64), gfx::ColorSpace::CreateSRGB(),
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SurfaceHandle(),
        SHARED_IMAGE_USAGE_GLES2_READ | SHARED_IMAGE_USAGE_GLES2_WRITE,
        "TestLabel");
    return mailbox;
  }

  void ReadColors(gl::GLApi* const api,
                  const Mailbox& mailbox,
                  std::array<SkColor, 4>& quadrants) {
    auto rep = ProduceGLTextureBase(mailbox);
    auto access = rep->BeginScopedAccess(
        GLTextureImageRepresentationBase::kReadAccessMode,
        GLTextureImageRepresentationBase::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(rep->IsCleared());

    GLuint fbo;
    api->glGenFramebuffersEXTFn(1, &fbo);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);
    api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D,
                                     rep->GetTextureBase()->service_id(), 0);

    api->glDisableFn(GL_SCISSOR_TEST);

    // Initialize to gray in case read pixels will fail.
    for (auto& color : quadrants)
      color = SK_ColorGRAY;

    api->glReadPixelsFn(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &quadrants[0]);
    api->glReadPixelsFn(48, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &quadrants[1]);
    api->glReadPixelsFn(16, 46, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &quadrants[2]);
    api->glReadPixelsFn(48, 48, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &quadrants[3]);

    // SkColor is BGRA and GL is RGBA, so we need to swap bytes.
    for (auto& color : quadrants) {
      color = SkColorSetARGB(SkColorGetA(color), SkColorGetB(color),
                             SkColorGetG(color), SkColorGetR(color));
    }

    api->glDeleteFramebuffersEXTFn(1, &fbo);
  }

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<SharedImageFactory> backing_factory_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<gles2::GLES2ExternalFramebuffer> gles2_external_framebuffer_;
  GLint max_sample_count_ = 0;
};

struct TestParams {
  explicit TestParams(const TestParamsTuple& params) {
    format = testing::get<0>(params);
    samples = testing::get<1>(params);
    preserve = testing::get<2>(params);
    need_depth = testing::get<3>(params);
    need_stencil = testing::get<4>(params);
  }

  viz::SharedImageFormat format;
  int samples;
  bool preserve;
  bool need_depth;
  bool need_stencil;
};

std::string TestParamToString(
    const testing::TestParamInfo<TestParamsTuple>& param_info) {
  auto params = TestParams(param_info.param);

  std::string result;
  result += params.format.ToString();

  if (params.samples)
    result += "Multisampling";

  if (params.preserve)
    result += "Preserve";
  if (params.need_depth)
    result += "Depth";
  if (params.need_stencil)
    result += "Stencil";

  return result;
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GLES2ExternalFrameBufferTest,
    ::testing::Combine(::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                                         viz::SinglePlaneFormat::kRGBX_8888),
                       ::testing::Values(0, 8),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool()),
    TestParamToString);

TEST_P(GLES2ExternalFrameBufferTest, Test) {
  auto params = TestParams(GetParam());
  auto mailbox1 = CreateSharedImage(params.format);

  gl::GLApi* const api = gl::g_current_gl_context;

  {
    GLint prev_fbo;
    api->glGetIntegervFn(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    EXPECT_TRUE(gles2_external_framebuffer_->AttachSharedImage(
        mailbox1, params.samples, params.preserve, params.need_depth,
        params.need_stencil));

    GLint new_fbo;
    api->glGetIntegervFn(GL_FRAMEBUFFER_BINDING, &new_fbo);

    // Verify that it doesn't update FBO binding
    EXPECT_EQ(new_fbo, prev_fbo);
  }

  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER,
                              gles2_external_framebuffer_->GetFramebufferId());
  api->glViewportFn(0, 0, 64, 64);

  GLint depth_bits = 0;
  GLint stencil_bits = 0;
  GLint alpha_bits = 0;

  api->glGetIntegervFn(GL_ALPHA_BITS, &alpha_bits);
  api->glGetIntegervFn(GL_DEPTH_BITS, &depth_bits);
  api->glGetIntegervFn(GL_STENCIL_BITS, &stencil_bits);

  // If we requested depth, expect it to be there.
  if (params.need_depth)
    EXPECT_GT(depth_bits, 0);

  // If we requested depth, expect it to be there.
  if (params.need_stencil)
    EXPECT_GT(stencil_bits, 0);

  // If we didn't request neither depth nor stencil. Note, that we prefer using
  // packed depth-stencil, so requesting one of them might have both.
  if (!params.need_depth && !params.need_stencil) {
    EXPECT_EQ(depth_bits, 0);
    EXPECT_EQ(stencil_bits, 0);
  }

  EXPECT_EQ(params.format.HasAlpha(), alpha_bits > 0);

  const bool draw_direct =
      !params.preserve && (std::min(max_sample_count_, params.samples) == 0);

  if (draw_direct) {
    auto rep = ProduceGLTextureBase(mailbox1);
    // AttachSharedImage should have cleared image if we draw directly.
    EXPECT_TRUE(rep->IsCleared());
  }

  api->glScissorFn(0, 0, 32, 32);
  api->glEnableFn(GL_SCISSOR_TEST);
  api->glClearColorFn(0.0, 1.0, 0.0, 1.0);  // Green
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  // Detach and resolve mailbox1 from the framebuffer. This always expected to
  // succeed.
  EXPECT_TRUE(gles2_external_framebuffer_->AttachSharedImage(
      Mailbox(), 0, false, false, false));

  const SkColor clear_color =
      params.format.HasAlpha() ? SK_ColorTRANSPARENT : SK_ColorBLACK;
  {
    std::array<SkColor, 4> colors;
    ReadColors(api, mailbox1, colors);
    // We draw this one.
    EXPECT_EQ(colors[0], SK_ColorGREEN);

    // We didn't draw those, so it should have been cleared to zero.
    EXPECT_EQ(colors[1], clear_color);
    EXPECT_EQ(colors[2], clear_color);
    EXPECT_EQ(colors[3], clear_color);
  }

  auto mailbox2 = CreateSharedImage(params.format);

  EXPECT_TRUE(gles2_external_framebuffer_->AttachSharedImage(
      mailbox2, params.samples, params.preserve, params.need_depth,
      params.need_stencil));
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER,
                              gles2_external_framebuffer_->GetFramebufferId());
  api->glViewportFn(0, 0, 64, 64);

  // Clear bottom right portion.
  api->glScissorFn(32, 32, 32, 32);
  api->glEnableFn(GL_SCISSOR_TEST);
  api->glClearColorFn(0.0, 0.0, 1.0, 1.0);  // Blue
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  // Detach and resolve mailbox1 from the framebuffer. This always expected to
  // succeed.
  EXPECT_TRUE(gles2_external_framebuffer_->AttachSharedImage(
      Mailbox(), 0, false, false, false));

  std::array<SkColor, 4> colors;
  ReadColors(api, mailbox2, colors);

  // If we requested to preserve content or if there is multisampling, the
  // contents of the back buffer is expected to stay, so top-left quadrant
  // should be green from the first draw.
  if (!draw_direct) {
    EXPECT_EQ(colors[0], SK_ColorGREEN);
  } else {
    EXPECT_EQ(colors[0], clear_color);
  }

  // We didn't draw those, so it should have been cleared to zero.
  EXPECT_EQ(colors[1], clear_color);
  EXPECT_EQ(colors[2], clear_color);

  // We just draw this one as blue.
  EXPECT_EQ(colors[3], SK_ColorBLUE);
}

}  // namespace
}  // namespace gpu
