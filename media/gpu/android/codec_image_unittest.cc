// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/codec_image.h"
#include "media/gpu/android/mock_texture_owner.h"
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

using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace media {

class CodecImageTest : public testing::Test {
 public:
  CodecImageTest() = default;

  void SetUp() override {
    auto codec = std::make_unique<NiceMock<MockMediaCodecBridge>>();
    codec_ = codec.get();
    wrapper_ = std::make_unique<CodecWrapper>(
        CodecSurfacePair(std::move(codec), new AVDASurfaceBundle()),
        base::DoNothing(), base::SequencedTaskRunnerHandle::Get());
    ON_CALL(*codec_, DequeueOutputBuffer(_, _, _, _, _, _, _))
        .WillByDefault(Return(MEDIA_CODEC_OK));

    gl::init::InitializeGLOneOffImplementation(gl::kGLImplementationEGLGLES2,
                                               false, false, false, false);
    surface_ = new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240));
    surface_->Initialize();
    share_group_ = new gl::GLShareGroup();
    context_ = new gl::GLContextEGL(share_group_.get());
    context_->Initialize(surface_.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    // The tests rely on this texture being bound.
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    texture_owner_ = new NiceMock<MockTextureOwner>(texture_id, context_.get(),
                                                    surface_.get());
  }

  void TearDown() override {
    context_ = nullptr;
    share_group_ = nullptr;
    surface_ = nullptr;
    gl::init::ShutdownGL(false);
    wrapper_->TakeCodecSurfacePair();
  }

  enum ImageKind { kOverlay, kTextureOwner };
  scoped_refptr<CodecImage> NewImage(
      ImageKind kind,
      CodecImage::DestructionCb destruction_cb = base::DoNothing()) {
    std::unique_ptr<CodecOutputBuffer> buffer;
    wrapper_->DequeueOutputBuffer(nullptr, nullptr, &buffer);
    scoped_refptr<CodecImage> image = new CodecImage(
        std::move(buffer), kind == kTextureOwner ? texture_owner_ : nullptr,
        base::BindRepeating(&PromotionHintReceiver::OnPromotionHint,
                            base::Unretained(&promotion_hint_receiver_)));

    image->SetDestructionCb(std::move(destruction_cb));
    return image;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  NiceMock<MockMediaCodecBridge>* codec_;
  std::unique_ptr<CodecWrapper> wrapper_;
  scoped_refptr<NiceMock<MockTextureOwner>> texture_owner_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;

  class PromotionHintReceiver {
   public:
    MOCK_METHOD1(OnPromotionHint, void(PromotionHintAggregator::Hint));
  };

  PromotionHintReceiver promotion_hint_receiver_;
};

TEST_F(CodecImageTest, DestructionCbRuns) {
  base::MockCallback<CodecImage::DestructionCb> cb;
  auto i = NewImage(kOverlay, cb.Get());
  EXPECT_CALL(cb, Run(i.get()));
  i = nullptr;
}

TEST_F(CodecImageTest, ImageStartsUnrendered) {
  auto i = NewImage(kTextureOwner);
  ASSERT_FALSE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, CopyTexImageIsInvalidForOverlayImages) {
  auto i = NewImage(kOverlay);
  ASSERT_FALSE(i->CopyTexImage(GL_TEXTURE_EXTERNAL_OES));
}

TEST_F(CodecImageTest, ScheduleOverlayPlaneIsInvalidForTextureOwnerImages) {
  auto i = NewImage(kTextureOwner);
  ASSERT_FALSE(i->ScheduleOverlayPlane(gfx::AcceleratedWidget(), 0,
                                       gfx::OverlayTransform(), gfx::Rect(),
                                       gfx::RectF(), true, nullptr));
}

TEST_F(CodecImageTest, CopyTexImageFailsIfTargetIsNotOES) {
  auto i = NewImage(kTextureOwner);
  ASSERT_FALSE(i->CopyTexImage(GL_TEXTURE_2D));
}

TEST_F(CodecImageTest, CopyTexImageFailsIfTheWrongTextureIsBound) {
  auto i = NewImage(kTextureOwner);
  GLuint wrong_texture_id;
  glGenTextures(1, &wrong_texture_id);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, wrong_texture_id);
  ASSERT_FALSE(i->CopyTexImage(GL_TEXTURE_EXTERNAL_OES));
}

TEST_F(CodecImageTest, CopyTexImageCanBeCalledRepeatedly) {
  auto i = NewImage(kTextureOwner);
  ASSERT_TRUE(i->CopyTexImage(GL_TEXTURE_EXTERNAL_OES));
  ASSERT_TRUE(i->CopyTexImage(GL_TEXTURE_EXTERNAL_OES));
}

TEST_F(CodecImageTest, CopyTexImageTriggersFrontBufferRendering) {
  auto i = NewImage(kTextureOwner);
  // Verify that the release comes before the wait.
  InSequence s;
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, true));
  EXPECT_CALL(*texture_owner_, WaitForFrameAvailable());
  EXPECT_CALL(*texture_owner_, UpdateTexImage());
  i->CopyTexImage(GL_TEXTURE_EXTERNAL_OES);
  ASSERT_TRUE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, GetTextureMatrixTriggersFrontBufferRendering) {
  auto i = NewImage(kTextureOwner);
  InSequence s;
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, true));
  EXPECT_CALL(*texture_owner_, WaitForFrameAvailable());
  EXPECT_CALL(*texture_owner_, UpdateTexImage());
  EXPECT_CALL(*texture_owner_, GetTransformMatrix(_));
  float matrix[16];
  i->GetTextureMatrix(matrix);
  ASSERT_TRUE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, GetTextureMatrixReturnsIdentityForOverlayImages) {
  auto i = NewImage(kOverlay);
  float matrix[16]{0};
  i->GetTextureMatrix(matrix);
  // See GetTextureMatrix() for the expected result.
  ASSERT_EQ(matrix[0], 1);
  ASSERT_EQ(matrix[5], -1);
}

TEST_F(CodecImageTest, ScheduleOverlayPlaneTriggersFrontBufferRendering) {
  auto i = NewImage(kOverlay);
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, true));
  // Also verify that it sends the appropriate promotion hint so that the
  // overlay is positioned properly.
  PromotionHintAggregator::Hint hint(gfx::Rect(1, 2, 3, 4), true);
  EXPECT_CALL(promotion_hint_receiver_, OnPromotionHint(hint));
  i->ScheduleOverlayPlane(gfx::AcceleratedWidget(), 0, gfx::OverlayTransform(),
                          hint.screen_rect, gfx::RectF(), true, nullptr);
  ASSERT_TRUE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, CanRenderTextureOwnerImageToBackBuffer) {
  auto i = NewImage(kTextureOwner);
  ASSERT_TRUE(i->RenderToTextureOwnerBackBuffer());
  ASSERT_FALSE(i->was_rendered_to_front_buffer());
}

TEST_F(CodecImageTest, CodecBufferInvalidationResultsInRenderingFailure) {
  auto i = NewImage(kTextureOwner);
  // Invalidate the backing codec buffer.
  wrapper_->TakeCodecSurfacePair();
  ASSERT_FALSE(i->RenderToTextureOwnerBackBuffer());
}

TEST_F(CodecImageTest, RenderToBackBufferDoesntWait) {
  auto i = NewImage(kTextureOwner);
  InSequence s;
  EXPECT_CALL(*codec_, ReleaseOutputBuffer(_, true));
  EXPECT_CALL(*texture_owner_, SetReleaseTimeToNow());
  EXPECT_CALL(*texture_owner_, WaitForFrameAvailable()).Times(0);
  ASSERT_TRUE(i->RenderToTextureOwnerBackBuffer());
}

TEST_F(CodecImageTest, PromotingTheBackBufferWaits) {
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*texture_owner_, SetReleaseTimeToNow()).Times(1);
  i->RenderToTextureOwnerBackBuffer();
  EXPECT_CALL(*texture_owner_, WaitForFrameAvailable());
  ASSERT_TRUE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, PromotingTheBackBufferAlwaysSucceeds) {
  auto i = NewImage(kTextureOwner);
  i->RenderToTextureOwnerBackBuffer();
  // Invalidating the codec buffer doesn't matter after it's rendered to the
  // back buffer.
  wrapper_->TakeCodecSurfacePair();
  ASSERT_TRUE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, FrontBufferRenderingFailsIfBackBufferRenderingFailed) {
  auto i = NewImage(kTextureOwner);
  wrapper_->TakeCodecSurfacePair();
  i->RenderToTextureOwnerBackBuffer();
  ASSERT_FALSE(i->RenderToFrontBuffer());
}

TEST_F(CodecImageTest, RenderToFrontBufferRestoresTextureBindings) {
  GLuint pre_bound_texture = 0;
  glGenTextures(1, &pre_bound_texture);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, pre_bound_texture);
  auto i = NewImage(kTextureOwner);
  EXPECT_CALL(*texture_owner_, UpdateTexImage());
  i->RenderToFrontBuffer();
  GLint post_bound_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &post_bound_texture);
  ASSERT_EQ(pre_bound_texture, static_cast<GLuint>(post_bound_texture));
}

TEST_F(CodecImageTest, RenderToFrontBufferRestoresGLContext) {
  // Make a new context current.
  scoped_refptr<gl::GLSurface> surface(
      new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240)));
  surface->Initialize();
  scoped_refptr<gl::GLShareGroup> share_group(new gl::GLShareGroup());
  scoped_refptr<gl::GLContext> context(new gl::GLContextEGL(share_group.get()));
  context->Initialize(surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(context->MakeCurrent(surface.get()));

  auto i = NewImage(kTextureOwner);
  // Our context should not be current when UpdateTexImage() is called.
  EXPECT_CALL(*texture_owner_, UpdateTexImage()).WillOnce(Invoke([&]() {
    ASSERT_FALSE(context->IsCurrent(surface.get()));
  }));
  i->RenderToFrontBuffer();
  // Our context should have been restored.
  ASSERT_TRUE(context->IsCurrent(surface.get()));

  context = nullptr;
  share_group = nullptr;
  surface = nullptr;
}

TEST_F(CodecImageTest, ScheduleOverlayPlaneDoesntSendDuplicateHints) {
  // SOP should send only one promotion hint unless the position changes.
  auto i = NewImage(kOverlay);
  // Also verify that it sends the appropriate promotion hint so that the
  // overlay is positioned properly.
  PromotionHintAggregator::Hint hint1(gfx::Rect(1, 2, 3, 4), true);
  PromotionHintAggregator::Hint hint2(gfx::Rect(5, 6, 7, 8), true);
  EXPECT_CALL(promotion_hint_receiver_, OnPromotionHint(hint1)).Times(1);
  EXPECT_CALL(promotion_hint_receiver_, OnPromotionHint(hint2)).Times(1);
  i->ScheduleOverlayPlane(gfx::AcceleratedWidget(), 0, gfx::OverlayTransform(),
                          hint1.screen_rect, gfx::RectF(), true, nullptr);
  i->ScheduleOverlayPlane(gfx::AcceleratedWidget(), 0, gfx::OverlayTransform(),
                          hint1.screen_rect, gfx::RectF(), true, nullptr);
  // Sending a different rectangle should send another hint.
  i->ScheduleOverlayPlane(gfx::AcceleratedWidget(), 0, gfx::OverlayTransform(),
                          hint2.screen_rect, gfx::RectF(), true, nullptr);
}

TEST_F(CodecImageTest, GetAHardwareBuffer) {
  auto i = NewImage(kTextureOwner);
  EXPECT_EQ(texture_owner_->get_a_hardware_buffer_count, 0);
  EXPECT_FALSE(i->was_rendered_to_front_buffer());

  EXPECT_CALL(*texture_owner_, UpdateTexImage());
  i->GetAHardwareBuffer();
  EXPECT_EQ(texture_owner_->get_a_hardware_buffer_count, 1);
  EXPECT_TRUE(i->was_rendered_to_front_buffer());
}

}  // namespace media
