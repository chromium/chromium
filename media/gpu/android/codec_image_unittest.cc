// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/mock_codec_buffer_wait_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

using testing::_;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace media {

constexpr gfx::Size kFrameSize(640, 480);

class CodecImageTest : public testing::Test {
 public:
  CodecImageTest() = default;

  void SetUp() override {
    auto codec = std::make_unique<NiceMock<MockMediaCodecBridge>>();
    codec_ = codec.get();
    wrapper_ = std::make_unique<CodecWrapper>(
        CodecSurfacePair(std::move(codec), new CodecSurfaceBundle()),
        base::DoNothing(), base::SequencedTaskRunner::GetCurrentDefault(),
        kFrameSize, gfx::ColorSpace::CreateREC709(), std::nullopt,
        /*use_block_model=*/false);
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault(Return(OkStatus()));

    gl::init::InitializeStaticGLBindingsImplementation(
        gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
    display_ = gl::init::InitializeGLOneOffPlatformImplementation(
        /*disable_gl_drawing=*/false,
        /*init_extensions=*/false,
        /*gpu_preference=*/gl::GpuPreference::kDefault);

    scoped_refptr<gl::GLSurface> surface(new gl::PbufferGLSurfaceEGL(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size(320, 240)));
    surface->Initialize();
    share_group_ = new gl::GLShareGroup();
    context_ = new gl::GLContextEGL(share_group_.get());
    context_->Initialize(surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_->default_surface());
    ASSERT_TRUE(context_->MakeCurrentDefault());

    glGenTextures(1, &texture_id_);
    // The tests rely on this texture being bound.
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id_);

    auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
        texture_id_, context_.get(), context_->default_surface(),
        BindsTextureOnUpdate());
    ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
        .WillByDefault(
            Invoke([](gfx::Size rotated_visible_size, gfx::Size* coded_size,
                      gfx::Rect* visible_rect) {
              *visible_rect = gfx::Rect(rotated_visible_size);
              *coded_size = visible_rect->size();
              return true;
            }));

    codec_buffer_wait_coordinator_ =
        base::MakeRefCounted<NiceMock<MockCodecBufferWaitCoordinator>>(
            std::move(texture_owner));
  }

  void TearDown() override {
    if (texture_id_ && context_->MakeCurrentDefault()) {
      glDeleteTextures(1, &texture_id_);
    }
    context_ = nullptr;
    share_group_ = nullptr;
    gl::init::ShutdownGL(display_, false);
    wrapper_->TakeCodecSurfacePair();
  }

  enum ImageKind { kOverlay, kTextureOwner };
  scoped_refptr<CodecImage> NewImage(
      ImageKind kind,
      CodecImage::UnusedCB unused_cb = base::DoNothing()) {
    std::unique_ptr<CodecOutputBuffer> buffer;
    wrapper_->DequeueOutputBuffer(nullptr, nullptr, &buffer);

    auto codec_buffer_wait_coordinator =
        kind == kTextureOwner ? codec_buffer_wait_coordinator_ : nullptr;
    auto buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
        std::move(buffer), codec_buffer_wait_coordinator,
        features::NeedThreadSafeAndroidMedia()
            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
            : nullptr);

    buffer_renderer->set_frame_info_callback(base::BindOnce(
        &CodecImageTest::OnFrameInfoReady, base::Unretained(this)));

    scoped_refptr<CodecImage> image =
        new CodecImage(buffer_renderer->size(),
                       features::NeedThreadSafeAndroidMedia()
                           ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
                           : nullptr);
    image->Initialize(
        std::move(buffer_renderer), kind == kTextureOwner,
        base::BindRepeating(&PromotionHintReceiver::OnPromotionHint,
                            base::Unretained(&promotion_hint_receiver_)));

    image->AddUnusedCB(std::move(unused_cb));
    return image;
  }

  MOCK_METHOD(void,
              OnFrameInfoReady,
              (std::optional<gfx::Size> coded_size,
               std::optional<gfx::Rect> visible_rect),
              ());

  virtual bool BindsTextureOnUpdate() { return true; }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<NiceMock<MockMediaCodecBridge>> codec_;
  std::unique_ptr<CodecWrapper> wrapper_;
  scoped_refptr<NiceMock<MockCodecBufferWaitCoordinator>>
      codec_buffer_wait_coordinator_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  GLuint texture_id_ = 0;
  raw_ptr<gl::GLDisplay> display_ = nullptr;

  class PromotionHintReceiver {
   public:
    MOCK_METHOD1(OnPromotionHint, void(PromotionHintAggregator::Hint));
  };

  PromotionHintReceiver promotion_hint_receiver_;
};

class CodecImageTestExplicitBind : public CodecImageTest {
  bool BindsTextureOnUpdate() override { return false; }
};

TEST_F(CodecImageTest, UnusedCBRunsOnDestruction) {
  // Add multiple UnusedCBs and verify that they are all run when the CodecImage
  // is destroyed.
  base::MockCallback<CodecImage::UnusedCB> cb_1;
  base::MockCallback<CodecImage::UnusedCB> cb_2;
  auto i = NewImage(kOverlay);
  i->AddUnusedCB(cb_1.Get());
  i->AddUnusedCB(cb_2.Get());
  EXPECT_CALL(cb_1, Run(i.get()));
  EXPECT_CALL(cb_2, Run(i.get()));
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
  i = nullptr;
}

TEST_F(CodecImageTest, UnusedCBRunsOnNotifyUnused) {
  base::MockCallback<CodecImage::UnusedCB> cb_1;
  base::MockCallback<CodecImage::UnusedCB> cb_2;
  auto i = NewImage(kTextureOwner);
  ASSERT_TRUE(i->get_codec_output_buffer_for_testing());
  ASSERT_TRUE(i->HasTextureOwner());
  i->AddUnusedCB(cb_1.Get());
  i->AddUnusedCB(cb_2.Get());
  EXPECT_CALL(cb_1, Run(i.get()));
  EXPECT_CALL(cb_2, Run(i.get()));
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));

  // Also verify that the output buffer and texture owner are released.
  i->NotifyUnused();
  EXPECT_FALSE(i->get_codec_output_buffer_for_testing());
  EXPECT_FALSE(i->HasTextureOwner());

  // Verify that an additional call doesn't crash.  It should do nothing.
  i->NotifyUnused();
}

TEST_F(CodecImageTest, ImageStartsUnrendered) {
  auto i = NewImage(kTextureOwner);
  ASSERT_FALSE(i->was_rendered_to_front_buffer());
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
}

TEST_F(CodecImageTest, CanRenderTextureOwnerImageToBackBuffer) {
  auto i = NewImage(kTextureOwner);
  ASSERT_TRUE(i->RenderToTextureOwnerBackBuffer());
  ASSERT_FALSE(i->was_rendered_to_front_buffer());
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
}

TEST_F(CodecImageTest, CodecBufferInvalidationResultsInRenderingFailure) {
  auto i = NewImage(kTextureOwner);
  // Invalidate the backing codec buffer.
  wrapper_->TakeCodecSurfacePair();
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
  ASSERT_FALSE(i->RenderToTextureOwnerBackBuffer());
}

TEST_F(CodecImageTest, RenderToBackBufferDoesntWait) {
  auto i = NewImage(kTextureOwner);
  InSequence s;
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, true));
  EXPECT_CALL(*codec_buffer_wait_coordinator_, SetReleaseTimeToNow());
  EXPECT_CALL(*codec_buffer_wait_coordinator_, WaitForFrameAvailable())
      .Times(0);
  ASSERT_TRUE(i->RenderToTextureOwnerBackBuffer());
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
}

TEST_F(CodecImageTest, PromotingTheBackBufferWaits) {
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*codec_buffer_wait_coordinator_, SetReleaseTimeToNow()).Times(1);
  i->RenderToTextureOwnerBackBuffer();
  EXPECT_CALL(*codec_buffer_wait_coordinator_, WaitForFrameAvailable());
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  ASSERT_TRUE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, PromotingTheBackBufferAlwaysSucceeds) {
  auto i = NewImage(kTextureOwner);
  i->RenderToTextureOwnerBackBuffer();
  // Invalidating the codec buffer doesn't matter after it's rendered to the
  // back buffer.
  wrapper_->TakeCodecSurfacePair();
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  ASSERT_TRUE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, FrontBufferRenderingFailsIfBackBufferRenderingFailed) {
  auto i = NewImage(kTextureOwner);
  wrapper_->TakeCodecSurfacePair();
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
  i->RenderToTextureOwnerBackBuffer();
  ASSERT_FALSE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, RenderToFrontBufferRestoresTextureBindings) {
  GLuint pre_bound_texture = 0;
  glGenTextures(1, &pre_bound_texture);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, pre_bound_texture);
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*codec_buffer_wait_coordinator_->texture_owner(),
              UpdateTexImage());
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  i->RenderToFrontBuffer();
  GLint post_bound_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &post_bound_texture);
  ASSERT_EQ(pre_bound_texture, static_cast<GLuint>(post_bound_texture));
}

TEST_F(CodecImageTestExplicitBind, RenderToFrontBufferDoesNotBindTexture) {
  GLuint pre_bound_texture = 0;
  glGenTextures(1, &pre_bound_texture);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, pre_bound_texture);
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*codec_buffer_wait_coordinator_->texture_owner(),
              UpdateTexImage());
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  i->RenderToFrontBuffer();
  GLint post_bound_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &post_bound_texture);
  ASSERT_EQ(pre_bound_texture, static_cast<GLuint>(post_bound_texture));
}

TEST_F(CodecImageTest, RenderToFrontBufferRestoresGLContext) {
  // Make a new context current.
  scoped_refptr<gl::GLSurface> surface(new gl::PbufferGLSurfaceEGL(
      gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size(320, 240)));
  surface->Initialize();
  scoped_refptr<gl::GLShareGroup> share_group(new gl::GLShareGroup());
  scoped_refptr<gl::GLContext> context(new gl::GLContextEGL(share_group.get()));
  context->Initialize(surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(context->default_surface());
  ASSERT_TRUE(context->MakeCurrentDefault());

  auto i = NewImage(kTextureOwner);
  // UpdateTexImage sets it's own context.
  EXPECT_CALL(*codec_buffer_wait_coordinator_->texture_owner(),
              UpdateTexImage());
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  i->RenderToFrontBuffer();
  // Our context should have been restored.
  ASSERT_TRUE(context->IsCurrent(surface.get()));

  context = nullptr;
  share_group = nullptr;
}

TEST_F(CodecImageTest, GetAHardwareBuffer) {
  auto i = NewImage(kTextureOwner);
  EXPECT_EQ(codec_buffer_wait_coordinator_->texture_owner()
                ->get_a_hardware_buffer_count,
            0);
  EXPECT_FALSE(i->was_rendered_to_front_buffer());

  EXPECT_CALL(*codec_buffer_wait_coordinator_->texture_owner(),
              UpdateTexImage());
  EXPECT_CALL(*this,
              OnFrameInfoReady(Eq(kFrameSize), Eq(gfx::Rect(kFrameSize))));
  i->GetAHardwareBuffer();
  EXPECT_EQ(codec_buffer_wait_coordinator_->texture_owner()
                ->get_a_hardware_buffer_count,
            1);
  EXPECT_TRUE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, GetAHardwareBufferAfterRelease) {
  // Make sure that we get a nullptr AHB once we've marked the image as unused.
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
  i->NotifyUnused();
  EXPECT_FALSE(i->GetAHardwareBuffer());
}

TEST_F(CodecImageTest, RenderAfterUnusedDoesntCrash) {
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*this, OnFrameInfoReady(Eq(std::nullopt), Eq(std::nullopt)));
  i->NotifyUnused();
  EXPECT_FALSE(i->RenderToTextureOwnerBackBuffer());
  EXPECT_FALSE(i->RenderToTextureOwnerFrontBuffer());
}

}  // namespace media
