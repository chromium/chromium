// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::Return;

namespace gpu {
namespace gles2 {
namespace {

const GLint kMaxTextureSize = 64;
const GLint kMaxCubemapSize = 64;
const GLint kMaxRectangleTextureSize = 64;
const GLint kMax3DTextureSize = 256;
const GLint kMaxArrayTextureLayers = 256;
const GLint kMaxRenderbufferSize = 64;
const GLint kMaxSamples = 4;
const uint32_t kMaxDrawBuffers = 16;
const uint32_t kMaxColorAttachments = 16;
const bool kUseDefaultTextures = false;

}  // namespace

class FramebufferManagerTest : public GpuServiceTest {
 public:
  FramebufferManagerTest()
      : manager_(1, 1, nullptr),
        feature_info_(new FeatureInfo()),
        discardable_manager_(GpuPreferences()) {
    texture_manager_.reset(new TextureManager(
        nullptr, feature_info_.get(), kMaxTextureSize, kMaxCubemapSize,
        kMaxRectangleTextureSize, kMax3DTextureSize, kMaxArrayTextureLayers,
        kUseDefaultTextures, nullptr, &discardable_manager_));
    renderbuffer_manager_.reset(new RenderbufferManager(nullptr,
                                                        kMaxRenderbufferSize,
                                                        kMaxSamples,
                                                        feature_info_.get()));
  }
  ~FramebufferManagerTest() override {
    manager_.Destroy(false);
    texture_manager_->MarkContextLost();
    texture_manager_->Destroy();
    renderbuffer_manager_->Destroy(false);
  }

 protected:
  FramebufferManager manager_;
  scoped_refptr<FeatureInfo> feature_info_;
  ServiceDiscardableManager discardable_manager_;
  std::unique_ptr<TextureManager> texture_manager_;
  std::unique_ptr<RenderbufferManager> renderbuffer_manager_;
};

TEST_F(FramebufferManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create framebuffer.
  manager_.CreateFramebuffer(kClient1Id, kService1Id);
  // Check framebuffer got created.
  Framebuffer* framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 != nullptr);
  EXPECT_FALSE(framebuffer1->IsDeleted());
  EXPECT_EQ(kService1Id, framebuffer1->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(framebuffer1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent framebuffer.
  EXPECT_TRUE(manager_.GetFramebuffer(kClient2Id) == nullptr);
  // Check trying to a remove non-existent framebuffers does not crash.
  manager_.RemoveFramebuffer(kClient2Id);
  // Check framebuffer gets deleted when last reference is released.
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  // Check we can't get the framebuffer after we remove it.
  manager_.RemoveFramebuffer(kClient1Id);
  EXPECT_TRUE(manager_.GetFramebuffer(kClient1Id) == nullptr);
}

TEST_F(FramebufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create framebuffer.
  manager_.CreateFramebuffer(kClient1Id, kService1Id);
  // Check framebuffer got created.
  Framebuffer* framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 != nullptr);
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 == nullptr);
}

class FramebufferInfoTestBase : public GpuServiceTest {
 public:
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  explicit FramebufferInfoTestBase(ContextType context_type)
      : context_type_(context_type),
        manager_(kMaxDrawBuffers,
                 kMaxColorAttachments,
                 &framebuffer_completeness_cache_),
        feature_info_(new FeatureInfo()),
        discardable_manager_(GpuPreferences()) {
    texture_manager_.reset(new TextureManager(
        nullptr, feature_info_.get(), kMaxTextureSize, kMaxCubemapSize,
        kMaxRectangleTextureSize, kMax3DTextureSize, kMaxArrayTextureLayers,
        kUseDefaultTextures, nullptr, &discardable_manager_));
    renderbuffer_manager_.reset(new RenderbufferManager(nullptr,
                                                        kMaxRenderbufferSize,
                                                        kMaxSamples,
                                                        feature_info_.get()));
  }
  ~FramebufferInfoTestBase() override {
    manager_.Destroy(false);
    texture_manager_->MarkContextLost();
    texture_manager_->Destroy();
    renderbuffer_manager_->Destroy(false);
  }

 protected:
  void SetUp() override {
    bool is_es3 = false;
    if (context_type_ == CONTEXT_TYPE_WEBGL2 ||
        context_type_ == CONTEXT_TYPE_OPENGLES3)
      is_es3 = true;
    InitializeContext(is_es3 ? "3.0" : "2.0", "GL_EXT_framebuffer_object");
  }

  void InitializeContext(const char* gl_version, const char* extensions) {
    GpuServiceTest::SetUpWithGLVersion(gl_version, extensions);
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(gl_.get(),
        extensions, "", gl_version, context_type_);
    feature_info_->InitializeForTesting(context_type_);
    decoder_.reset(
        new MockGLES2Decoder(&client_, &command_buffer_service_, &outputter_));
    manager_.CreateFramebuffer(kClient1Id, kService1Id);
    error_state_.reset(new ::testing::StrictMock<gles2::MockErrorState>());
    framebuffer_ = manager_.GetFramebuffer(kClient1Id);
    ASSERT_TRUE(framebuffer_ != nullptr);
  }

  ContextType context_type_;
  FramebufferCompletenessCache framebuffer_completeness_cache_;
  FramebufferManager manager_;
  Framebuffer* framebuffer_;
  scoped_refptr<FeatureInfo> feature_info_;
  ServiceDiscardableManager discardable_manager_;
  std::unique_ptr<TextureManager> texture_manager_;
  std::unique_ptr<RenderbufferManager> renderbuffer_manager_;
  std::unique_ptr<MockErrorState> error_state_;
  FakeCommandBufferServiceBase command_buffer_service_;
  FakeDecoderClient client_;
  TraceOutputter outputter_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
};

class FramebufferInfoTest : public FramebufferInfoTestBase {
 public:
  FramebufferInfoTest() : FramebufferInfoTestBase(CONTEXT_TYPE_OPENGLES2) {}
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint FramebufferInfoTestBase::kClient1Id;
const GLuint FramebufferInfoTestBase::kService1Id;
#endif

TEST_F(FramebufferInfoTest, Basic) {
  EXPECT_EQ(kService1Id, framebuffer_->service_id());
  EXPECT_FALSE(framebuffer_->IsDeleted());
  EXPECT_TRUE(nullptr == framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(nullptr == framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(nullptr == framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
}

TEST_F(FramebufferInfoTest, AttachRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;
  const GLuint kRenderbufferClient3Id = 35;
  const GLuint kRenderbufferService3Id = 335;
  const GLuint kRenderbufferClient4Id = 36;
  const GLuint kRenderbufferService4Id = 336;
  const GLuint kRenderbufferClient5Id = 37;
  const GLuint kRenderbufferService5Id = 337;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLenum kFormat1 = GL_RGBA4;
  const GLenum kBadFormat1 = GL_DEPTH_COMPONENT16;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 16;
  const GLsizei kHeight2 = 32;
  const GLenum kFormat2 = GL_DEPTH_COMPONENT16;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 16;
  const GLsizei kHeight3 = 32;
  const GLenum kFormat3 = GL_STENCIL_INDEX8;
  const GLsizei kSamples3 = 0;
  const GLsizei kWidth4 = 16;
  const GLsizei kHeight4 = 32;
  const GLenum kFormat4 = GL_DEPTH24_STENCIL8;
  const GLsizei kSamples4 = 0;
  const GLsizei kWidth5 = 16;
  const GLsizei kHeight5 = 32;
  const GLenum kFormat5 = GL_DEPTH24_STENCIL8;
  const GLsizei kSamples5 = 0;
  const GLsizei kDifferentSamples5 = 1;

  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != nullptr);

  // Check adding one attachment.
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, renderbuffer1);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA4),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Try a format that's not good for COLOR_ATTACHMENT0.
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer1, kSamples1,
                                              kBadFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Try a good format.
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer1, kSamples1,
                                              kFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());

  // Check adding another.
  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  Renderbuffer* renderbuffer2 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient2Id);
  ASSERT_TRUE(renderbuffer2 != nullptr);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer2);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  // The attachment has a size of 0,0 so depending on the order of the map
  // of attachments it could either get INCOMPLETE_ATTACHMENT because it's 0,0
  // or INCOMPLETE_DIMENSIONS because it's not the same size as the other
  // attachment.
  GLenum status = framebuffer_->IsPossiblyComplete(feature_info_.get());
  EXPECT_TRUE(
      status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ||
      status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT);
  EXPECT_FALSE(framebuffer_->IsCleared());

  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer2, kSamples2,
                                              kFormat2, kWidth2, kHeight2);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));

  // Check marking them as cleared.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, renderbuffer_manager_.get(), texture_manager_.get());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Add another one to stencil attachment point.
  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient3Id, kRenderbufferService3Id);
  Renderbuffer* renderbuffer3 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient3Id);
  ASSERT_TRUE(renderbuffer3 != nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer3, kSamples3,
                                              kFormat3, kWidth3, kHeight3);
  renderbuffer_manager_->SetCleared(renderbuffer3, true);

  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, renderbuffer3);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_TRUE(framebuffer_->HasStencilAttachment());
  // Binding different images to depth and stencil attachment points should
  // return FRAMEBUFFER_UNSUPPORTED.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_UNSUPPORTED),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Bind a renderbufer in format DEPTH_STENCIL to depth and stencil
  // attachment points.
  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient4Id, kRenderbufferService4Id);
  Renderbuffer* renderbuffer4 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient4Id);
  ASSERT_TRUE(renderbuffer4 != nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer4, kSamples4,
                                              kFormat4, kWidth4, kHeight4);
  renderbuffer_manager_->SetCleared(renderbuffer4, true);

  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer4);
  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, renderbuffer4);
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_TRUE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Check marking the renderbuffer as uncleared.
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer1, kSamples1,
                                              kFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_TRUE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());

  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());

  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));

  // Clear it.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, renderbuffer_manager_.get(), texture_manager_.get());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Check replacing one attachment when both depth and stencil attachments
  // are present.
  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient5Id, kRenderbufferService5Id);
  Renderbuffer* renderbuffer5 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient5Id);
  ASSERT_TRUE(renderbuffer5 != nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer5, kSamples5,
                                              kFormat5, kWidth5, kHeight5);

  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, renderbuffer5);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->IsCleared());

  attachment = framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth5, attachment->width());
  EXPECT_EQ(kHeight5, attachment->height());
  EXPECT_EQ(kSamples5, attachment->samples());
  EXPECT_EQ(kFormat5, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  // Binding different images to depth and stencil attachment points should
  // return FRAMEBUFFER_UNSUPPORTED.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_UNSUPPORTED),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Check replacing both depth and stencil attachments.
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer5);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->IsCleared());

  attachment = framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT);
  EXPECT_EQ(kWidth5, attachment->width());
  EXPECT_EQ(kHeight5, attachment->height());
  EXPECT_EQ(kSamples5, attachment->samples());
  EXPECT_EQ(kFormat5, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Change samples.
  ASSERT_FALSE(
      feature_info_->feature_flags().chromium_framebuffer_mixed_samples);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer5, kDifferentSamples5,
                                              kFormat5, kWidth5, kHeight5);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer5, kSamples5,
                                              kFormat5, kWidth5, kHeight5);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Check changing an attachment.
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer5, kSamples5,
                                              kFormat5, kWidth5 + 1, kHeight5);

  attachment = framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth5 + 1, attachment->width());
  EXPECT_EQ(kHeight5, attachment->height());
  EXPECT_EQ(kSamples5, attachment->samples());
  EXPECT_EQ(kFormat5, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Check removing it.
  // Restore the width of renderbuffer5 to avoid INCOMPLETE_DIMENSIONS_EXT.
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer5, kSamples5,
                                              kFormat5, kWidth5, kHeight5);

  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, nullptr);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Remove depth, Set color to 0 size.
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer1, kSamples1,
                                              kFormat1, 0, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Remove color.
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

TEST_F(FramebufferInfoTest, AttachTexture2D) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLint kLevel1 = 0;
  const GLenum kFormat1 = GL_RGBA;
  const GLenum kBadFormat1 = GL_DEPTH_COMPONENT16;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 16;
  const GLsizei kHeight2 = 32;
  const GLint kLevel2 = 0;
  const GLenum kFormat2 = GL_RGB;
  const GLenum kTarget2 = GL_TEXTURE_2D;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 75;
  const GLsizei kHeight3 = 123;
  const GLint kLevel3 = 0;
  const GLenum kFormat3 = GL_RGBA;
  const GLsizei kSamples3 = 0;
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  texture_manager_->CreateTexture(kTextureClient1Id, kTextureService1Id);
  scoped_refptr<TextureRef> texture1(
      texture_manager_->GetTexture(kTextureClient1Id));
  ASSERT_TRUE(texture1.get() != nullptr);

  // check adding one attachment
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture1.get(), kTarget1, kLevel1, kSamples1);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());

  // Try format that doesn't work with COLOR_ATTACHMENT0
  texture_manager_->SetTarget(texture1.get(), GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(
      texture1.get(), GL_TEXTURE_2D, kLevel1, kBadFormat1, kWidth1, kHeight1,
      kDepth, kBorder, kBadFormat1, kType, gfx::Rect(kWidth1, kHeight1));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Try a good format.
  texture_manager_->SetLevelInfo(texture1.get(), GL_TEXTURE_2D, kLevel1,
                                 kFormat1, kWidth1, kHeight1, kDepth, kBorder,
                                 kFormat1, kType, gfx::Rect());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());
  texture_manager_->SetLevelInfo(texture1.get(), GL_TEXTURE_2D, kLevel1,
                                 kFormat1, kWidth1, kHeight1, kDepth, kBorder,
                                 kFormat1, kType, gfx::Rect(kWidth1, kHeight1));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetReadBufferInternalFormat());

  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check replacing an attachment
  texture_manager_->CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<TextureRef> texture2(
      texture_manager_->GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2.get() != nullptr);
  texture_manager_->SetTarget(texture2.get(), GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(texture2.get(), GL_TEXTURE_2D, kLevel2,
                                 kFormat2, kWidth2, kHeight2, kDepth, kBorder,
                                 kFormat2, kType, gfx::Rect(kWidth2, kHeight2));

  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2.get(), kTarget2, kLevel2, kSamples2);
  EXPECT_EQ(static_cast<GLenum>(kFormat2),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());

  attachment = framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth2, attachment->width());
  EXPECT_EQ(kHeight2, attachment->height());
  EXPECT_EQ(kSamples2, attachment->samples());
  EXPECT_EQ(kFormat2, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check changing attachment
  texture_manager_->SetLevelInfo(texture2.get(), GL_TEXTURE_2D, kLevel3,
                                 kFormat3, kWidth3, kHeight3, kDepth, kBorder,
                                 kFormat3, kType, gfx::Rect());
  attachment = framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);
  EXPECT_EQ(kWidth3, attachment->width());
  EXPECT_EQ(kHeight3, attachment->height());
  EXPECT_EQ(kSamples3, attachment->samples());
  EXPECT_EQ(kFormat3, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat3),
            framebuffer_->GetReadBufferInternalFormat());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());

  // Set to size 0
  texture_manager_->SetLevelInfo(texture2.get(), GL_TEXTURE_2D, kLevel3,
                                 kFormat3, 0, 0, kDepth, kBorder, kFormat3,
                                 kType, gfx::Rect());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  // Check removing it.
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT0, nullptr, 0, 0, 0);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == nullptr);
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
}

TEST_F(FramebufferInfoTest, AttachTextureCube) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth = 16;
  const GLsizei kHeight = 16;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGBA;
  const GLenum kTarget = GL_TEXTURE_CUBE_MAP;
  const GLsizei kSamples = 0;

  const GLenum kTexTargets[] = {
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get());

  texture_manager_->SetTarget(texture.get(), kTarget);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), kTexTargets[0], kLevel, kSamples);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());

  texture_manager_->SetLevelInfo(texture.get(), kTexTargets[0], kLevel,
                                 kFormat, kWidth, kHeight, kDepth, kBorder,
                                 kFormat, kType, gfx::Rect(kWidth, kHeight));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat),
            framebuffer_->GetReadBufferInternalFormat());
  // Cube incomplete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  for (size_t ii = 1; ii < 6; ++ii) {
    texture_manager_->SetLevelInfo(texture.get(), kTexTargets[ii], kLevel,
                                   kFormat, kWidth, kHeight, kDepth, kBorder,
                                   kFormat, kType, gfx::Rect(kWidth, kHeight));
  }
  // Cube complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

TEST_F(FramebufferInfoTest, AttachTextureLayer) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth = 16;
  const GLsizei kHeight = 32;
  const GLint kDepth = 2;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGBA;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLsizei kLayer = 0;
  const GLint kWrongLayer = kDepth;

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get());

  framebuffer_->AttachTextureLayer(
      GL_COLOR_ATTACHMENT0, texture.get(), kTarget, kLevel, kWrongLayer);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());

  texture_manager_->SetTarget(texture.get(), kTarget);
  texture_manager_->SetLevelInfo(texture.get(), kTarget, kLevel,
                                 kFormat, kWidth, kHeight, kDepth, kBorder,
                                 kFormat, kType, gfx::Rect());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  framebuffer_->AttachTextureLayer(
      GL_COLOR_ATTACHMENT0, texture.get(), kTarget, kLevel, kLayer);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat),
            framebuffer_->GetReadBufferInternalFormat());

  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment);
  EXPECT_EQ(kWidth, attachment->width());
  EXPECT_EQ(kHeight, attachment->height());
  EXPECT_EQ(kFormat, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
}

TEST_F(FramebufferInfoTest, ClearPartiallyClearedAttachments) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), GL_TEXTURE_2D, 0, 0);
  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);

  // Not cleared at all.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4,
                                 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  // Clear it but nothing happens.
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());

  // Fully cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4,
                                 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(0, 0, 4, 4));
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
  // Clear it but nothing happens.
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());

  // Partially cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4,
                                 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(1, 1, 2, 2));
  EXPECT_FALSE(attachment->cleared());
  EXPECT_TRUE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  // Now clear it.
  EXPECT_CALL(*decoder_.get(), ClearLevel(texture->texture(),
                                          GL_TEXTURE_2D,
                                          0,
                                          GL_RGBA,
                                          GL_UNSIGNED_BYTE,
                                          _, _, _, _))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
}

TEST_F(FramebufferInfoTest, Clear3DTextureAttachments) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_3D);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), GL_TEXTURE_3D, 0, 0);
  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);

  const int kWidth = 4;
  const int kHeight = 8;
  const int kDepth = 2;

  // Fully cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA8,
                                 kWidth, kHeight, kDepth,
                                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(0, 0, kWidth, kHeight));
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
  // Clear it but nothing happens.
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());

  // Not cleared at all.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_3D, 0, GL_RGBA8,
                                 kWidth, kHeight, kDepth,
                                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  // Now clear it.
  EXPECT_CALL(*decoder_.get(), ClearLevel3D(texture->texture(),
                                            GL_TEXTURE_3D,
                                            0,
                                            GL_RGBA,
                                            GL_UNSIGNED_BYTE,
                                            kWidth, kHeight, kDepth))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
}

TEST_F(FramebufferInfoTest, Clear3DOutsideRenderableRange) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_3D);
  // Set base level to 1 but attach level 0.
  TestHelper::SetTexParameteriWithExpectations(gl_.get(),
                                               error_state_.get(),
                                               texture_manager_.get(),
                                               texture.get(),
                                               GL_TEXTURE_BASE_LEVEL,
                                               1,
                                               GL_NO_ERROR);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), GL_TEXTURE_3D, 0, 0);
  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);

  // Level 0 is not cleared at all.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_3D, 0, GL_RGBA, 4,
                                 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect());
  // Level 1 is cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_3D, 1, GL_RGBA, 2,
                                 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(0, 0, 2, 2));
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  EXPECT_CALL(*decoder_.get(),
              ClearLevel3D(texture->texture(), GL_TEXTURE_3D, 0, GL_RGBA,
                           GL_UNSIGNED_BYTE, _, _, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
}

TEST_F(FramebufferInfoTest, ClearIntegerTextureAttachments) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), GL_TEXTURE_2D, 0, 0);
  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);

  const int kWidth = 4;
  const int kHeight = 8;

  // Fully cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA8UI,
                                 kWidth, kHeight, 1,
                                 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                                 gfx::Rect(0, 0, kWidth, kHeight));
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
  // Clear it but nothing happens.
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());

  // Not cleared at all.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA8UI,
                                 kWidth, kHeight, 1,
                                 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                                 gfx::Rect());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  // Now clear it.
  EXPECT_CALL(*decoder_.get(), IsCompressedTextureFormat(GL_RGBA8UI))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(*decoder_.get(), ClearLevel(texture->texture(),
                                          GL_TEXTURE_2D,
                                          0,
                                          GL_RGBA_INTEGER,
                                          GL_UNSIGNED_BYTE,
                                          0, 0, kWidth, kHeight))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
}

TEST_F(FramebufferInfoTest, ClearIntegerOutsideRenderableRange) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
  // Set base level to 1 but attach level 0.
  TestHelper::SetTexParameteriWithExpectations(gl_.get(),
                                               error_state_.get(),
                                               texture_manager_.get(),
                                               texture.get(),
                                               GL_TEXTURE_BASE_LEVEL,
                                               1,
                                               GL_NO_ERROR);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), GL_TEXTURE_2D, 0, 0);
  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != nullptr);

  // Level 0 is not cleared at all.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0, GL_RGBA8UI, 4,
                                 4, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                                 gfx::Rect());
  // Level 1 is cleared.
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 1, GL_RGBA8UI, 2,
                                 2, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,
                                 gfx::Rect(0, 0, 2, 2));
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());
  EXPECT_CALL(*decoder_.get(), IsCompressedTextureFormat(GL_RGBA8UI))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(*decoder_.get(),
              ClearLevel(texture->texture(), GL_TEXTURE_2D, 0, GL_RGBA_INTEGER,
                         GL_UNSIGNED_BYTE, _, _, _, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  framebuffer_->ClearUnclearedIntOr3DTexturesOrPartiallyClearedTextures(
      decoder_.get(), texture_manager_.get());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_FALSE(attachment->IsPartiallyCleared());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());
}

TEST_F(FramebufferInfoTest, DrawBuffers) {
  const GLuint kTextureClientId[] = { 33, 34 };
  const GLuint kTextureServiceId[] = { 333, 334 };
  for (GLenum i = GL_COLOR_ATTACHMENT0;
       i < GL_COLOR_ATTACHMENT0 + kMaxColorAttachments; ++i) {
    EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(i));
  }
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());

  EXPECT_EQ(static_cast<GLenum>(GL_COLOR_ATTACHMENT0),
            framebuffer_->GetDrawBuffer(GL_DRAW_BUFFER0_ARB));
  for (GLenum i = GL_DRAW_BUFFER1_ARB;
       i < GL_DRAW_BUFFER0_ARB + kMaxDrawBuffers; ++i) {
    EXPECT_EQ(static_cast<GLenum>(GL_NONE),
              framebuffer_->GetDrawBuffer(i));
  }

  for (size_t ii = 0; ii < base::size(kTextureClientId); ++ii) {
    texture_manager_->CreateTexture(
        kTextureClientId[ii], kTextureServiceId[ii]);
    scoped_refptr<TextureRef> texture(
        texture_manager_->GetTexture(kTextureClientId[ii]));
    ASSERT_TRUE(texture.get());

    framebuffer_->AttachTexture(
        GL_COLOR_ATTACHMENT0 + ii, texture.get(), GL_TEXTURE_2D, 0, 0);
    EXPECT_FALSE(
        framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0 + ii));

    const Framebuffer::Attachment* attachment =
        framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0 + ii);
    ASSERT_TRUE(attachment);
    EXPECT_TRUE(attachment->cleared());
  }
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_FALSE(framebuffer_->HasUnclearedColorAttachments());

  // Set draw buffer 1 as uncleared.
  scoped_refptr<TextureRef> texture1(
      texture_manager_->GetTexture(kTextureClientId[1]));
  texture_manager_->SetTarget(texture1.get(), GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(texture1.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4,
                                 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect());

  const Framebuffer::Attachment* attachment1 =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT1);
  ASSERT_TRUE(attachment1);
  EXPECT_FALSE(attachment1->cleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT1));
  EXPECT_TRUE(framebuffer_->HasUnclearedColorAttachments());

  GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
  framebuffer_->SetDrawBuffers(2, buffers);
  EXPECT_EQ(static_cast<GLenum>(GL_COLOR_ATTACHMENT0),
            framebuffer_->GetDrawBuffer(GL_DRAW_BUFFER0_ARB));
  EXPECT_EQ(static_cast<GLenum>(GL_COLOR_ATTACHMENT1),
            framebuffer_->GetDrawBuffer(GL_DRAW_BUFFER1_ARB));
  for (GLenum i = GL_DRAW_BUFFER2_ARB;
       i < GL_DRAW_BUFFER0_ARB + kMaxDrawBuffers; ++i) {
    EXPECT_EQ(static_cast<GLenum>(GL_NONE),
              framebuffer_->GetDrawBuffer(i));
  }

  // Only draw buffer 1 needs clearing, so we need to mask draw buffer 0.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(
      framebuffer_->PrepareDrawBuffersForClearingUninitializedAttachments());

  // Now we disable draw buffer 1.
  buffers[1] = GL_NONE;
  framebuffer_->SetDrawBuffers(2, buffers);
  // We will enable the disabled draw buffer for clear(), and disable it
  // after the clear.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(
      framebuffer_->PrepareDrawBuffersForClearingUninitializedAttachments());

  // Now we disable draw buffer 0, enable draw buffer 1.
  buffers[0] = GL_NONE;
  buffers[1] = GL_COLOR_ATTACHMENT1;
  framebuffer_->SetDrawBuffers(2, buffers);
  // This is the perfect setting for clear. No need to call DrawBuffers().
  EXPECT_FALSE(
      framebuffer_->PrepareDrawBuffersForClearingUninitializedAttachments());
}

TEST_F(FramebufferInfoTest, DrawBufferMasks) {
  const GLuint kTextureClientId[] = { 33, 34, 35, 36, 37 };
  const GLuint kTextureServiceId[] = { 333, 334, 335, 336, 337 };
  const GLenum kAttachment[] = {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2,
      GL_COLOR_ATTACHMENT4,
      GL_DEPTH_ATTACHMENT};
  const GLenum kInternalFormat[] = {
      GL_RGBA8,
      GL_RG32UI,
      GL_R16I,
      GL_R16F,
      GL_DEPTH_COMPONENT24};
  const GLenum kFormat[] = {
      GL_RGBA,
      GL_RG_INTEGER,
      GL_RED_INTEGER,
      GL_RED,
      GL_DEPTH_COMPONENT};
  const GLenum kType[] = {
      GL_UNSIGNED_BYTE,
      GL_UNSIGNED_INT,
      GL_SHORT,
      GL_FLOAT,
      GL_UNSIGNED_INT};

  for (size_t ii = 0; ii < base::size(kTextureClientId); ++ii) {
    texture_manager_->CreateTexture(
        kTextureClientId[ii], kTextureServiceId[ii]);
    scoped_refptr<TextureRef> texture(
        texture_manager_->GetTexture(kTextureClientId[ii]));
    ASSERT_TRUE(texture.get());
    texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
    texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, 0,
                                   kInternalFormat[ii], 4, 4, 1, 0,
                                   kFormat[ii], kType[ii], gfx::Rect());
    framebuffer_->AttachTexture(
        kAttachment[ii], texture.get(), GL_TEXTURE_2D, 0, 0);
    ASSERT_TRUE(framebuffer_->GetAttachment(kAttachment[ii]));
  }

  manager_.MarkAsComplete(framebuffer_);

  {  // Default draw buffer settings
    EXPECT_EQ(0x3u, framebuffer_->draw_buffer_type_mask());
    EXPECT_EQ(0x3u, framebuffer_->draw_buffer_bound_mask());
    EXPECT_FALSE(framebuffer_->ContainsActiveIntegerAttachments());
  }

  {  // Exact draw buffer settings.
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                        GL_COLOR_ATTACHMENT2, GL_NONE, GL_COLOR_ATTACHMENT4};
    framebuffer_->SetDrawBuffers(5, buffers);
    EXPECT_EQ(0x31Bu, framebuffer_->draw_buffer_type_mask());
    EXPECT_EQ(0x33Fu, framebuffer_->draw_buffer_bound_mask());
    EXPECT_TRUE(framebuffer_->ContainsActiveIntegerAttachments());
  }

  {  // All disabled draw buffer settings.
    GLenum buffers[] = {GL_NONE};
    framebuffer_->SetDrawBuffers(1, buffers);
    EXPECT_EQ(0u, framebuffer_->draw_buffer_type_mask());
    EXPECT_EQ(0u, framebuffer_->draw_buffer_bound_mask());
    EXPECT_FALSE(framebuffer_->ContainsActiveIntegerAttachments());
  }

  {  // Filter out integer buffers.
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE,
                        GL_COLOR_ATTACHMENT4};
    framebuffer_->SetDrawBuffers(5, buffers);
    EXPECT_EQ(0x303u, framebuffer_->draw_buffer_type_mask());
    EXPECT_EQ(0x303u, framebuffer_->draw_buffer_bound_mask());
    EXPECT_FALSE(framebuffer_->ContainsActiveIntegerAttachments());
  }

  {  // All enabled draw buffer settings.
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                        GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
                        GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
                        GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7};
    framebuffer_->SetDrawBuffers(8, buffers);
    EXPECT_EQ(0x31Bu, framebuffer_->draw_buffer_type_mask());
    EXPECT_EQ(0x33Fu, framebuffer_->draw_buffer_bound_mask());
    EXPECT_TRUE(framebuffer_->ContainsActiveIntegerAttachments());
  }

  // Test ValidateAndAdjustDrawBuffers().

  // gl_FragColor situation.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _)).Times(0);
  EXPECT_FALSE(framebuffer_->ValidateAndAdjustDrawBuffers(0x3u, 0x3u));
  // gl_FragData situation.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _))
      .Times(0);
  EXPECT_FALSE(
      framebuffer_->ValidateAndAdjustDrawBuffers(0xFFFFFFFFu, 0xFFFFFFFFu));
  // User defined output variables, fully match.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _)).Times(0);
  EXPECT_TRUE(
      framebuffer_->ValidateAndAdjustDrawBuffers(0x31Bu, 0x33Fu));
  // User defined output variables, fully on, one type mismatch.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _))
      .Times(0);
  EXPECT_FALSE(
      framebuffer_->ValidateAndAdjustDrawBuffers(0x32Bu, 0x33Fu));
  // Empty output.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _)).Times(0);
  EXPECT_FALSE(framebuffer_->ValidateAndAdjustDrawBuffers(0u, 0u));
  // User defined output variables, some active buffers have no corresponding
  // output variables, but if they do, types match.
  EXPECT_CALL(*gl_, DrawBuffersARB(kMaxDrawBuffers, _)).Times(0);
  EXPECT_FALSE(framebuffer_->ValidateAndAdjustDrawBuffers(0x310u, 0x330u));
}

class FramebufferInfoFloatTest : public FramebufferInfoTestBase {
 public:
  FramebufferInfoFloatTest()
      : FramebufferInfoTestBase(CONTEXT_TYPE_OPENGLES3) {}
  ~FramebufferInfoFloatTest() override = default;

 protected:
  void SetUp() override {
    InitializeContext("OpenGL ES 3.0",
        "GL_OES_texture_float GL_EXT_color_buffer_float");
  }
};

TEST_F(FramebufferInfoFloatTest, AttachFloatTexture) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLenum kType = GL_FLOAT;
  const GLsizei kWidth = 16;
  const GLsizei kHeight = 32;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGBA;
  const GLenum kInternalFormat = GL_RGBA32F;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLsizei kSamples = 0;
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);

  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), kTarget, kLevel, kSamples);
  EXPECT_EQ(static_cast<GLenum>(0),
            framebuffer_->GetReadBufferInternalFormat());

  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, kLevel,
                                 kInternalFormat, kWidth, kHeight, kDepth,
                                 kBorder, kFormat, kType, gfx::Rect());
  // Texture with a sized float internalformat is allowed as an attachment
  // since float color attachment extension is present.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

TEST_F(FramebufferInfoTest, UnbindRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != nullptr);
  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  Renderbuffer* renderbuffer2 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient2Id);
  ASSERT_TRUE(renderbuffer2 != nullptr);

  // Attach to 2 attachment points.
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, renderbuffer1);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  // Check they were attached.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != nullptr);
  // Unbind unattached renderbuffer.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer2);
  // Should be no-op.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != nullptr);
  // Unbind renderbuffer.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  // Check they were detached
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) == nullptr);
}

TEST_F(FramebufferInfoTest, UnbindTexture) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;
  const GLint kSamples1 = 0;

  texture_manager_->CreateTexture(kTextureClient1Id, kTextureService1Id);
  scoped_refptr<TextureRef> texture1(
      texture_manager_->GetTexture(kTextureClient1Id));
  ASSERT_TRUE(texture1.get() != nullptr);
  texture_manager_->CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<TextureRef> texture2(
      texture_manager_->GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2.get() != nullptr);

  // Attach to 2 attachment points.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture1.get(), kTarget1, kLevel1, kSamples1);
  framebuffer_->AttachTexture(
      GL_DEPTH_ATTACHMENT, texture1.get(), kTarget1, kLevel1, kSamples1);
  // Check they were attached.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != nullptr);
  // Unbind unattached texture.
  framebuffer_->UnbindTexture(kTarget1, texture2.get());
  // Should be no-op.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != nullptr);
  // Unbind texture.
  framebuffer_->UnbindTexture(kTarget1, texture1.get());
  // Check they were detached
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == nullptr);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) == nullptr);
}

TEST_F(FramebufferInfoTest, LastColorAttachmentIdTest) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLuint kTextureClient3Id = 35;
  const GLuint kTextureService3Id = 335;
  const GLuint kRenderbufferClientId = 36;
  const GLuint kRenderbufferServiceId = 336;
  const GLuint kTextureLayerClientId = 37;
  const GLuint kTextureLayerServiceId = 337;

  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;
  const GLint kSamples1 = 0;

  const GLenum kTargetTextureLayer = GL_TEXTURE_2D_ARRAY;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth = 16;
  const GLsizei kHeight = 32;
  const GLint kDepth = 2;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGBA;
  const GLsizei kLayer = 0;

  texture_manager_->CreateTexture(kTextureClient1Id, kTextureService1Id);
  scoped_refptr<TextureRef> texture1(
      texture_manager_->GetTexture(kTextureClient1Id));
  ASSERT_TRUE(texture1.get() != nullptr);
  texture_manager_->CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<TextureRef> texture2(
      texture_manager_->GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2.get() != nullptr);
  texture_manager_->CreateTexture(kTextureClient3Id, kTextureService3Id);
  scoped_refptr<TextureRef> texture3(
      texture_manager_->GetTexture(kTextureClient3Id));
  ASSERT_TRUE(texture3.get() != nullptr);

  renderbuffer_manager_->CreateRenderbuffer(kRenderbufferClientId,
                                            kRenderbufferServiceId);
  Renderbuffer* renderbuffer =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClientId);
  ASSERT_TRUE(renderbuffer != nullptr);

  texture_manager_->CreateTexture(kTextureLayerClientId,
                                  kTextureLayerServiceId);
  scoped_refptr<TextureRef> textureLayer(
      texture_manager_->GetTexture(kTextureLayerClientId));
  ASSERT_TRUE(textureLayer.get());

  texture_manager_->SetTarget(textureLayer.get(), kTargetTextureLayer);
  texture_manager_->SetLevelInfo(textureLayer.get(), kTargetTextureLayer,
                                 kLevel, kFormat, kWidth, kHeight, kDepth,
                                 kBorder, kFormat, kType, gfx::Rect());

  EXPECT_EQ(framebuffer_->last_color_attachment_id(), -1);
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT0, texture1.get(), kTarget1,
                              kLevel1, kSamples1);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 0);
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT2, texture3.get(), kTarget1,
                              kLevel1, kSamples1);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 2);
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT1, texture2.get(), kTarget1,
                              kLevel1, kSamples1);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 2);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 2);
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT3, renderbuffer);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 3);
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT4, texture1.get(), kTarget1,
                              kLevel1, kSamples1);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 4);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != nullptr);
  framebuffer_->AttachTextureLayer(GL_COLOR_ATTACHMENT5, textureLayer.get(),
                                   kTargetTextureLayer, kLevel, kLayer);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 5);

  framebuffer_->UnbindTexture(kTargetTextureLayer, textureLayer.get());
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 4);
  framebuffer_->UnbindTexture(kTarget1, texture2.get());
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 4);
  framebuffer_->UnbindTexture(kTarget1, texture1.get());
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 3);
  framebuffer_->UnbindRenderbuffer(GL_COLOR_ATTACHMENT3, renderbuffer);
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), 2);
  framebuffer_->UnbindTexture(kTarget1, texture3.get());
  EXPECT_EQ(framebuffer_->last_color_attachment_id(), -1);
}

TEST_F(FramebufferInfoTest, IsCompleteMarkAsComplete) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;
  const GLint kSamples1 = 0;

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != nullptr);
  texture_manager_->CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<TextureRef> texture2(
      texture_manager_->GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2.get() != nullptr);

  // Check MarkAsComlete marks as complete.
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));

  // Check at attaching marks as not complete.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2.get(), kTarget1, kLevel1, kSamples1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));

  // Check MarkAttachmentsAsCleared marks as complete.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, renderbuffer_manager_.get(), texture_manager_.get());
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));

  // Check Unbind marks as not complete.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));
  framebuffer_->UnbindTexture(kTarget1, texture2.get());
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
}

TEST_F(FramebufferInfoTest, GetStatus) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;
  const GLint kSamples1 = 0;

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != nullptr);
  texture_manager_->CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<TextureRef> texture2(
      texture_manager_->GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2.get() != nullptr);
  texture_manager_->SetTarget(texture2.get(), GL_TEXTURE_2D);

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(texture_manager_.get(), GL_FRAMEBUFFER);

  // Check a second call for the same type does not call anything
  framebuffer_->GetStatus(texture_manager_.get(), GL_FRAMEBUFFER);

  // Check changing the attachments calls CheckFramebufferStatus.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2.get(), kTarget1, kLevel1, kSamples1);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE)).RetiresOnSaturation();
  framebuffer_->GetStatus(texture_manager_.get(), GL_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  framebuffer_->GetStatus(texture_manager_.get(), GL_FRAMEBUFFER);

  // Check a second call with a different target calls CheckFramebufferStatus.
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check adding another attachment calls CheckFramebufferStatus.
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check changing the format calls CheckFramebuffferStatus.
  TestHelper::SetTexParameteriWithExpectations(gl_.get(),
                                               error_state_.get(),
                                               texture_manager_.get(),
                                               texture2.get(),
                                               GL_TEXTURE_WRAP_S,
                                               GL_CLAMP_TO_EDGE,
                                               GL_NO_ERROR);

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check since it did not return FRAMEBUFFER_COMPLETE that it calls
  // CheckFramebufferStatus
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check putting it back does not call CheckFramebufferStatus.
  TestHelper::SetTexParameteriWithExpectations(gl_.get(),
                                               error_state_.get(),
                                               texture_manager_.get(),
                                               texture2.get(),
                                               GL_TEXTURE_WRAP_S,
                                               GL_REPEAT,
                                               GL_NO_ERROR);
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);

  // Check Unbinding does not call CheckFramebufferStatus
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  framebuffer_->GetStatus(texture_manager_.get(), GL_READ_FRAMEBUFFER);
}

class FramebufferInfoES3Test : public FramebufferInfoTestBase {
 public:
  FramebufferInfoES3Test() : FramebufferInfoTestBase(CONTEXT_TYPE_WEBGL2) {}

 protected:
  void SetUp() override {
    InitializeContext("OpenGL ES 3.0", "");
  }
};

TEST_F(FramebufferInfoES3Test, DifferentDimensions) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLenum kFormat1 = GL_RGBA4;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 32;  // Different from kWidth1
  const GLsizei kHeight2 = 32;
  const GLenum kFormat2 = GL_DEPTH_COMPONENT16;
  const GLsizei kSamples2 = 0;

  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer1, kSamples1,
                                              kFormat1, kWidth1, kHeight1);
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, renderbuffer1);

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  Renderbuffer* renderbuffer2 =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClient2Id);
  ASSERT_TRUE(renderbuffer2 != nullptr);
  renderbuffer_manager_->SetInfoAndInvalidate(renderbuffer2, kSamples2,
                                              kFormat2, kWidth2, kHeight2);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer2);

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

TEST_F(FramebufferInfoES3Test, DuplicatedAttachments) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kWidth = 16;
  const GLint kHeight = 32;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLint kSamples = 0;

  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get() != nullptr);
  texture_manager_->SetTarget(texture.get(), GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(texture.get(), GL_TEXTURE_2D, kLevel,
                                 kFormat, kWidth, kHeight, kDepth, kBorder,
                                 kFormat, kType, gfx::Rect());

  // Check an image is attached to more than one color attachment point
  // in a framebuffer.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture.get(), kTarget, kLevel, kSamples);
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT1, texture.get(), kTarget, kLevel, kSamples);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_UNSUPPORTED),
      framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

TEST_F(FramebufferInfoES3Test, ReadBuffer) {
  const GLuint kRenderbufferClientId = 33;
  const GLuint kRenderbufferServiceId = 333;

  EXPECT_EQ(static_cast<GLenum>(GL_COLOR_ATTACHMENT0),
            framebuffer_->read_buffer());
  framebuffer_->set_read_buffer(GL_NONE);
  EXPECT_EQ(static_cast<GLenum>(GL_NONE), framebuffer_->read_buffer());
  EXPECT_FALSE(framebuffer_->GetReadBufferAttachment());

  framebuffer_->set_read_buffer(GL_COLOR_ATTACHMENT1);
  EXPECT_FALSE(framebuffer_->GetReadBufferAttachment());

  renderbuffer_manager_->CreateRenderbuffer(
      kRenderbufferClientId, kRenderbufferServiceId);
  Renderbuffer* renderbuffer =
      renderbuffer_manager_->GetRenderbuffer(kRenderbufferClientId);
  ASSERT_TRUE(renderbuffer != nullptr);
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT1, renderbuffer);
  EXPECT_TRUE(framebuffer_->GetReadBufferAttachment());
}

TEST_F(FramebufferInfoES3Test, AttachNonLevel0Texture) {
  const GLuint kTextureClientId = 33;
  const GLuint kTextureServiceId = 333;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth = 16;
  const GLsizei kHeight = 32;
  const GLint kLevel = 2;
  const GLenum kInternalFormat = GL_RGBA8;
  const GLenum kFormat = GL_RGBA;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLsizei kSamples = 0;

  texture_manager_->CreateTexture(kTextureClientId, kTextureServiceId);
  scoped_refptr<TextureRef> texture(
      texture_manager_->GetTexture(kTextureClientId));
  ASSERT_TRUE(texture.get());

  texture_manager_->SetTarget(texture.get(), kTarget);
  texture_manager_->SetLevelInfo(texture.get(), kTarget, kLevel,
                                 kInternalFormat, kWidth, kHeight, 0, kBorder,
                                 kFormat, kType, gfx::Rect());

  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT0, texture.get(), kTarget,
                              kLevel, kSamples);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));

  EXPECT_CALL(*gl_, TexParameteri(kTarget, GL_TEXTURE_BASE_LEVEL, kLevel))
      .Times(1)
      .RetiresOnSaturation();
  texture_manager_->SetParameteri("FramebufferInfoTest.AttachNonLevel0Texturer",
                                  error_state_.get(), texture.get(),
                                  GL_TEXTURE_BASE_LEVEL, kLevel);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete(feature_info_.get()));
}

}  // namespace gles2
}  // namespace gpu
