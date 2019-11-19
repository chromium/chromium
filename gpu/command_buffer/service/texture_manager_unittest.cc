// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gl_stream_texture_image_stub.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_switches.h"

using ::testing::AtLeast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace gpu {
namespace gles2 {

class TextureTestHelper {
 public:
  static bool IsNPOT(const Texture* texture) {
    return texture->npot();
  }
  static bool IsTextureComplete(const Texture* texture) {
    return texture->texture_complete();
  }
  static bool IsCubeComplete(const Texture* texture) {
    return texture->cube_complete();
  }
  static GLuint owned_service_id(const Texture* texture) {
    return texture->owned_service_id();
  }
};

class TextureManagerTest : public GpuServiceTest {
 public:
  static const GLint kMaxTextureSize = 32;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMaxRectangleTextureSize = 32;
  static const GLint kMaxExternalTextureSize = 32;
  static const GLint kMax3DTextureSize = 512;
  static const GLint kMaxArrayTextureLayers = 256;
  static const GLint kMax2dLevels = 6;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLint kMaxExternalLevels = 1;
  static const GLint kMax3dLevels = 10;
  static const bool kUseDefaultTextures = false;

  TextureManagerTest() {
    GpuDriverBugWorkarounds gpu_driver_bug_workaround;
    feature_info_ =
        new FeatureInfo(gpu_driver_bug_workaround, GpuFeatureInfo());
  }

  ~TextureManagerTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    manager_.reset(new TextureManager(
        nullptr, feature_info_.get(), kMaxTextureSize, kMaxCubeMapTextureSize,
        kMaxRectangleTextureSize, kMax3DTextureSize, kMaxArrayTextureLayers,
        kUseDefaultTextures, nullptr, &discardable_manager_));
    SetupFeatureInfo("", "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);
    TestHelper::SetupTextureManagerInitExpectations(
        gl_.get(), false, false, false, {}, kUseDefaultTextures);
    manager_->Initialize();
    error_state_.reset(new ::testing::StrictMock<MockErrorState>());
  }

  void TearDown() override {
    manager_->MarkContextLost();
    manager_->Destroy();
    manager_.reset();
    GpuServiceTest::TearDown();
  }

  void SetParameter(
      TextureRef* texture_ref, GLenum pname, GLint value, GLenum error) {
    TestHelper::SetTexParameteriWithExpectations(
        gl_.get(), error_state_.get(), manager_.get(),
        texture_ref, pname, value, error);
  }

  void SetupFeatureInfo(const char* gl_extensions,
                        const char* gl_version,
                        ContextType context_type) {
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), gl_extensions, "", gl_version, context_type);
    feature_info_->InitializeForTesting(context_type);
    ASSERT_TRUE(feature_info_->context_type() == context_type);
    if (feature_info_->IsWebGL2OrES3Context()) {
      EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, _))
          .WillOnce(SetArgPointee<1>(8))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_DRAW_BUFFERS, _))
          .WillOnce(SetArgPointee<1>(8))
          .RetiresOnSaturation();
      feature_info_->EnableES3Validators();
    }
  }

  scoped_refptr<FeatureInfo> feature_info_;
  ServiceDiscardableManager discardable_manager_;
  std::unique_ptr<TextureManager> manager_;
  std::unique_ptr<MockErrorState> error_state_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint TextureManagerTest::kMaxTextureSize;
const GLint TextureManagerTest::kMaxCubeMapTextureSize;
const GLint TextureManagerTest::kMaxRectangleTextureSize;
const GLint TextureManagerTest::kMaxExternalTextureSize;
const GLint TextureManagerTest::kMax3DTextureSize;
const GLint TextureManagerTest::kMaxArrayTextureLayers;
const GLint TextureManagerTest::kMax2dLevels;
const GLint TextureManagerTest::kMaxCubeMapLevels;
const GLint TextureManagerTest::kMaxExternalLevels;
const GLint TextureManagerTest::kMax3dLevels;
#endif

TEST_F(TextureManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  // Check we can create texture.
  manager_->CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  scoped_refptr<TextureRef> texture = manager_->GetTexture(kClient1Id);
  ASSERT_TRUE(texture.get() != nullptr);
  EXPECT_EQ(kService1Id, texture->service_id());
  EXPECT_EQ(kClient1Id, texture->client_id());
  EXPECT_EQ(texture->texture(), manager_->GetTextureForServiceId(
      texture->service_id()));
  // Check we get nothing for a non-existent texture.
  EXPECT_TRUE(manager_->GetTexture(kClient2Id) == nullptr);
  // Check trying to a remove non-existent textures does not crash.
  manager_->RemoveTexture(kClient2Id);
  // Check that it gets deleted when the last reference is released.
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  // Check we can't get the texture after we remove it.
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(manager_->GetTexture(kClient1Id) == nullptr);
  EXPECT_EQ(0u, texture->client_id());
}

TEST_F(TextureManagerTest, SetParameter) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create texture.
  manager_->CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  TextureRef* texture_ref = manager_->GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  manager_->SetTarget(texture_ref, GL_TEXTURE_2D);
  SetParameter(texture_ref, GL_TEXTURE_MIN_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(texture_ref, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->mag_filter());
  SetParameter(texture_ref, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_s());
  SetParameter(texture_ref, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_t());
  SetParameter(texture_ref, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1, GL_NO_ERROR);
  SetParameter(texture_ref, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2, GL_NO_ERROR);
  SetParameter(
      texture_ref, GL_TEXTURE_MIN_FILTER, GL_CLAMP_TO_EDGE, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(
      texture_ref, GL_TEXTURE_MAG_FILTER, GL_CLAMP_TO_EDGE, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(texture_ref, GL_TEXTURE_WRAP_S, GL_NEAREST, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_s());
  SetParameter(texture_ref, GL_TEXTURE_WRAP_T, GL_NEAREST, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_t());
  SetParameter(texture_ref, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0, GL_INVALID_VALUE);
  SetParameter(texture_ref, GL_TEXTURE_IMMUTABLE_FORMAT, 0, GL_INVALID_ENUM);
  SetParameter(texture_ref, GL_TEXTURE_IMMUTABLE_LEVELS, 0, GL_INVALID_ENUM);
}

TEST_F(TextureManagerTest, UseDefaultTexturesTrue) {
  bool use_default_textures = true;
  TestHelper::SetupTextureManagerInitExpectations(
      gl_.get(), false, false, false, {"GL_ANGLE_texture_usage"},
      use_default_textures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         use_default_textures, nullptr, &discardable_manager_);
  manager.Initialize();

  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_2D) != nullptr);
  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP) != nullptr);

  // TODO(vmiura): Test GL_TEXTURE_EXTERNAL_OES & GL_TEXTURE_RECTANGLE_ARB.

  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, UseDefaultTexturesFalse) {
  bool use_default_textures = false;
  TestHelper::SetupTextureManagerInitExpectations(
      gl_.get(), false, false, false, {"GL_ANGLE_texture_usage"},
      use_default_textures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         use_default_textures, nullptr, &discardable_manager_);
  manager.Initialize();

  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_2D) == nullptr);
  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP) == nullptr);

  // TODO(vmiura): Test GL_TEXTURE_EXTERNAL_OES & GL_TEXTURE_RECTANGLE_ARB.

  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, UseDefaultTexturesTrueES3) {
  bool use_default_textures = true;
  SetupFeatureInfo("", "OpenGL ES 3.0", CONTEXT_TYPE_OPENGLES3);
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), true, true, false,
                                                  {}, use_default_textures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         use_default_textures, nullptr, &discardable_manager_);
  manager.Initialize();

  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_3D) != nullptr);
  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_2D_ARRAY) != nullptr);

  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, UseDefaultTexturesFalseES3) {
  bool use_default_textures = false;
  SetupFeatureInfo("", "OpenGL ES 3.0", CONTEXT_TYPE_OPENGLES3);
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), true, true, false,
                                                  {}, use_default_textures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         use_default_textures, nullptr, &discardable_manager_);
  manager.Initialize();

  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_3D) == nullptr);
  EXPECT_TRUE(manager.GetDefaultTextureInfo(GL_TEXTURE_2D_ARRAY) == nullptr);

  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, TextureUsageExt) {
  TestHelper::SetupTextureManagerInitExpectations(
      gl_.get(), false, false, false, {"GL_ANGLE_texture_usage"},
      kUseDefaultTextures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.Initialize();
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create texture.
  manager.CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  TestHelper::SetTexParameteriWithExpectations(
      gl_.get(), error_state_.get(), &manager, texture_ref,
      GL_TEXTURE_USAGE_ANGLE, GL_FRAMEBUFFER_ATTACHMENT_ANGLE, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_ATTACHMENT_ANGLE),
            texture_ref->texture()->usage());
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  TestHelper::SetupTextureManagerInitExpectations(
      gl_.get(), false, false, false, {}, kUseDefaultTextures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.Initialize();
  // Check we can create texture.
  manager.CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  TextureRef* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != nullptr);
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  TestHelper::SetupTextureManagerDestructionExpectations(
      gl_.get(), false, false, {}, kUseDefaultTextures);
  manager.Destroy();
  // Check that resources got freed.
  texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture == nullptr);
}

TEST_F(TextureManagerTest, MaxValues) {
  // Check we get the right values for the max sizes.
  EXPECT_EQ(kMax2dLevels, manager_->MaxLevelsForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_X));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_X));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Y));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Z));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
  EXPECT_EQ(kMaxExternalLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_EXTERNAL_OES));
  EXPECT_EQ(kMax2dLevels, manager_->MaxLevelsForTarget(GL_TEXTURE_2D_ARRAY));
  EXPECT_EQ(kMax3dLevels, manager_->MaxLevelsForTarget(GL_TEXTURE_3D));
  EXPECT_EQ(kMaxTextureSize, manager_->MaxSizeForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_X));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_X));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Y));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Z));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
  EXPECT_EQ(kMaxRectangleTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_RECTANGLE_ARB));
  EXPECT_EQ(kMaxExternalTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_EXTERNAL_OES));
  EXPECT_EQ(kMaxTextureSize, manager_->MaxSizeForTarget(GL_TEXTURE_2D_ARRAY));
  EXPECT_EQ(kMax3DTextureSize, manager_->MaxSizeForTarget(GL_TEXTURE_3D));
  EXPECT_EQ(kMaxArrayTextureLayers, manager_->max_array_texture_layers());
}

TEST_F(TextureManagerTest, ValidForTarget) {
  // check 2d
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D, 0, kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 1, 1, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 1, 2, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 2, 1, 1));
  // check level out of range.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels, kMaxTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels, kMaxTextureSize, 1, 2));
  // Check NPOT width on level 0
  EXPECT_TRUE(manager_->ValidForTarget(GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager_->ValidForTarget(GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_FALSE(manager_->ValidForTarget(GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_FALSE(manager_->ValidForTarget(GL_TEXTURE_2D, 1, 2, 5, 1));

  // check array textures.
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D_ARRAY, 0, kMaxTextureSize, kMaxTextureSize,
      kMaxArrayTextureLayers));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D_ARRAY, 1, kMaxTextureSize, kMaxTextureSize,
      kMaxArrayTextureLayers));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D_ARRAY, 1, kMaxTextureSize / 2, kMaxTextureSize / 2,
      kMaxArrayTextureLayers));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D_ARRAY, kMax2dLevels - 1, 0, 0, kMaxArrayTextureLayers));

  // check cube
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, 0,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1, 1, 1, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1, 2, 2, 1));
  // check level out of range.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check not square.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 2));

  for (GLint level = 0; level < kMax2dLevels; ++level) {
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, kMaxTextureSize >> level, 1, 1));
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, 1, kMaxTextureSize >> level, 1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, (kMaxTextureSize >> level) + 1, 1, 1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, 1, (kMaxTextureSize >> level) + 1, 1));
  }

  for (GLint level = 0; level < kMaxCubeMapLevels; ++level) {
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_CUBE_MAP, level,
        kMaxCubeMapTextureSize >> level,
        kMaxCubeMapTextureSize >> level,
        1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_CUBE_MAP, level,
        (kMaxCubeMapTextureSize >> level) * 2,
        (kMaxCubeMapTextureSize >> level) * 2,
        1));
  }
}

TEST_F(TextureManagerTest, ValidForTargetNPOT) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  // Check NPOT width on level 0
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 1, 2, 5, 1));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureManagerTest, AlphaLuminanceCompatibilityProfile) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 11;

  SetupFeatureInfo("", "2.1", CONTEXT_TYPE_OPENGLES2);
  TestHelper::SetupTextureManagerInitExpectations(
      gl_.get(), false, false, false, {}, kUseDefaultTextures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.Initialize();

  // Create a texture.
  manager.CreateTexture(kClientId, kServiceId);
  scoped_refptr<TextureRef> texture_ref(manager.GetTexture(kClientId));
  manager.SetTarget(texture_ref.get(), GL_TEXTURE_2D);

  Texture* texture = texture_ref->texture();

  // GL_ALPHA emulation
  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 1,
      0, GL_ALPHA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  // GL_LUMINANCE emulation
  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1,
      1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  // GL_LUMINANCE_ALPHA emulation
  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
      1, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kServiceId)))
      .Times(1)
      .RetiresOnSaturation();
  manager.RemoveTexture(kClientId);
}

TEST_F(TextureManagerTest, AlphaLuminanceCoreProfileEmulation) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 11;

  SetupFeatureInfo("", "4.2", CONTEXT_TYPE_OPENGLES3);
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), true, true, true,
                                                  {}, kUseDefaultTextures);
  TextureManager manager(nullptr, feature_info_.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.Initialize();

  // Create a texture.
  manager.CreateTexture(kClientId, kServiceId);
  scoped_refptr<TextureRef> texture_ref(manager.GetTexture(kClientId));
  manager.SetTarget(texture_ref.get(), GL_TEXTURE_2D);

  Texture* texture = texture_ref->texture();

  // GL_ALPHA emulation
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_NONE))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_NONE))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_NONE))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED))
      .Times(1)
      .RetiresOnSaturation();

  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_ALPHA, 1, 1, 1,
      0, GL_ALPHA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  // GL_LUMINANCE emulation
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE))
      .Times(1)
      .RetiresOnSaturation();

  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_LUMINANCE, 1, 1,
      1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  // GL_LUMINANCE_ALPHA emulation
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A,
              GL_GREEN))
      .Times(1)
      .RetiresOnSaturation();

  manager.SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
      1, 1, 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  texture->ApplyFormatWorkarounds(feature_info_.get());

  // Ensure explicitly setting swizzles while using emulated settings properly
  // swizzles the swizzle.
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R,
              GL_GREEN))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A,
              GL_RED))
      .Times(1)
      .RetiresOnSaturation();

  manager.SetParameteri("TexParameteri", error_state_.get(), texture_ref.get(),
      GL_TEXTURE_SWIZZLE_R, GL_ALPHA);
  manager.SetParameteri("TexParameteri", error_state_.get(), texture_ref.get(),
      GL_TEXTURE_SWIZZLE_A, GL_GREEN);

  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kServiceId)))
      .Times(1)
      .RetiresOnSaturation();
  manager.RemoveTexture(kClientId);
}

class TextureTestBase : public GpuServiceTest {
 public:
  static const GLint kMaxTextureSize = 32;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMaxRectangleTextureSize = 32;
  static const GLint kMax3DTextureSize = 512;
  static const GLint kMaxArrayTextureLayers = 256;
  static const GLint kMax2dLevels = 6;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;
  static const bool kUseDefaultTextures = false;

  TextureTestBase()
      : feature_info_(new FeatureInfo()) {
  }
  ~TextureTestBase() override { texture_ref_ = nullptr; }

 protected:
  void SetUpBase(MemoryTracker* memory_tracker, const std::string& extensions) {
    GpuServiceTest::SetUp();
    TestHelper::SetupFeatureInfoInitExpectations(gl_.get(),
                                                 extensions.c_str());
    feature_info_->InitializeForTesting();

    manager_.reset(new TextureManager(
        memory_tracker, feature_info_.get(), kMaxTextureSize,
        kMaxCubeMapTextureSize, kMaxRectangleTextureSize, kMax3DTextureSize,
        kMaxArrayTextureLayers, kUseDefaultTextures, nullptr,
        &discardable_manager_));
    decoder_.reset(new ::testing::StrictMock<MockGLES2Decoder>(
        &client_, &command_buffer_service_, &outputter_));
    error_state_.reset(new ::testing::StrictMock<MockErrorState>());
    manager_->CreateTexture(kClient1Id, kService1Id);
    texture_ref_ = manager_->GetTexture(kClient1Id);
    ASSERT_TRUE(texture_ref_.get() != nullptr);
  }

  void TearDown() override {
    if (texture_ref_.get()) {
      // If it's not in the manager then setting texture_ref_ to nullptr will
      // delete the texture.
      if (!texture_ref_->client_id()) {
        // Check that it gets deleted when the last reference is released.
        EXPECT_CALL(*gl_,
            DeleteTextures(1, ::testing::Pointee(texture_ref_->service_id())))
            .Times(1)
            .RetiresOnSaturation();
      }
      texture_ref_ = nullptr;
    }
    manager_->MarkContextLost();
    manager_->Destroy();
    manager_.reset();
    GpuServiceTest::TearDown();
  }

  void SetParameter(
      TextureRef* texture_ref, GLenum pname, GLint value, GLenum error) {
    TestHelper::SetTexParameteriWithExpectations(
        gl_.get(), error_state_.get(), manager_.get(),
        texture_ref, pname, value, error);
  }

  FakeCommandBufferServiceBase command_buffer_service_;
  FakeDecoderClient client_;
  TraceOutputter outputter_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
  std::unique_ptr<MockErrorState> error_state_;
  scoped_refptr<FeatureInfo> feature_info_;
  ServiceDiscardableManager discardable_manager_;
  std::unique_ptr<TextureManager> manager_;
  scoped_refptr<TextureRef> texture_ref_;
};

class TextureTest : public TextureTestBase {
 protected:
  void SetUp() override { SetUpBase(nullptr, std::string()); }
};

class TextureMemoryTrackerTest : public TextureTestBase {
 protected:
  void SetUp() override { SetUpBase(&mock_memory_tracker_, std::string()); }

  StrictMock<MockMemoryTracker> mock_memory_tracker_;
};

#define EXPECT_MEMORY_ALLOCATION_CHANGE(old_size, new_size)    \
  EXPECT_CALL(mock_memory_tracker_,                            \
              TrackMemoryAllocatedChange(new_size - old_size)) \
      .Times(1)                                                \
      .RetiresOnSaturation()

TEST_F(TextureTest, Basic) {
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(0u, texture->target());
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_EQ(0, texture->num_uncleared_mips());
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(texture->IsImmutable());
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST_MIPMAP_LINEAR),
            texture->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_LINEAR), texture->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), texture->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), texture->wrap_t());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_EQ(0u, texture->estimated_size());
}

TEST_F(TextureTest, SetTargetTexture2D) {
  Texture* texture = texture_ref_->texture();
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(texture->IsImmutable());
}

TEST_F(TextureTest, SetTargetTextureExternalOES) {
  Texture* texture = texture_ref_->texture();
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_TRUE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_TRUE(texture->IsImmutable());
}

TEST_F(TextureTest, ZeroSizeCanNotRender2D) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureTest, ZeroSizeCanNotRenderExternalOES) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                         GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(1, 1));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                         GL_RGBA, 0, 0, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect());
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureTest, CanRenderTo) {
  TestHelper::SetupFeatureInfoInitExpectations(gl_.get(), "");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 1,
                         0, GL_RGB, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  EXPECT_TRUE(texture_ref_->texture()->CanRenderTo(feature_info.get(), 0));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_TRUE(texture_ref_->texture()->CanRenderTo(feature_info.get(), 0));
}

TEST_F(TextureTest, CanNotRenderTo) {
  TestHelper::SetupFeatureInfoInitExpectations(gl_.get(), "");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_LUMINANCE, 1,
                         1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                         gfx::Rect(1, 1));
  EXPECT_FALSE(texture_ref_->texture()->CanRenderTo(feature_info.get(), 0));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0,
                         GL_LUMINANCE_ALPHA, 0, 0, 1, 0, GL_LUMINANCE_ALPHA,
                         GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(texture_ref_->texture()->CanRenderTo(feature_info.get(), 0));
}

TEST_F(TextureTest, EstimatedSize) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 4));
  EXPECT_EQ(8u * 4u * 4u, texture_ref_->texture()->estimated_size());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 4));
  EXPECT_EQ(8u * 4u * 4u * 2u, texture_ref_->texture()->estimated_size());
}

TEST_F(TextureMemoryTrackerTest, EstimatedSize) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 4));
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 256);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 4));
  // Add expectation for texture deletion.
  EXPECT_MEMORY_ALLOCATION_CHANGE(256, 0);
}

TEST_F(TextureMemoryTrackerTest, LightweightRef) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 64);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));

  Texture* texture = texture_ref_->texture();
  MemoryTypeTracker* old_tracker = texture->GetMemTracker();

  EXPECT_MEMORY_ALLOCATION_CHANGE(64, 0);
  texture->SetLightweightRef();
  EXPECT_EQ(nullptr, texture->GetMemTracker());

  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 64);
  texture->RemoveLightweightRef(true);
  EXPECT_EQ(old_tracker, texture->GetMemTracker());

  EXPECT_MEMORY_ALLOCATION_CHANGE(64, 0);
  texture->SetLightweightRef();
  EXPECT_EQ(nullptr, texture->GetMemTracker());

  manager_->RemoveTexture(texture_ref_->client_id());
  texture_ref_ = nullptr;

  EXPECT_CALL(*gl_,
              DeleteTextures(1, ::testing::Pointee(texture->service_id())))
      .Times(1)
      .RetiresOnSaturation();
  texture->RemoveLightweightRef(true);
}

TEST_F(TextureTest, POT2D) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Check Setting level 0 to POT
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  EXPECT_TRUE(texture->SafeToRenderFrom());
  // Set filters to something that will work with a single mip.
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  // Set them back.
  SetParameter(texture_ref_.get(),
               GL_TEXTURE_MIN_FILTER,
               GL_LINEAR_MIPMAP_LINEAR,
               GL_NO_ERROR);

  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  // Make mips.
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  // Change a mip.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  // Make mips.
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
}

TEST_F(TextureTest, BaseLevel) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Check Setting level 1 to POT
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_BASE_LEVEL, 1, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureTest, BaseLevelMaxLevel) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Set up level 2, 3, 4.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 2, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 8));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 4, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR,
      GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MAG_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_BASE_LEVEL, 2, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MAX_LEVEL, 4, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_BASE_LEVEL, 0, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureMemoryTrackerTest, MarkMipmapsGenerated) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 64);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_MEMORY_ALLOCATION_CHANGE(64, 0);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 80);
  EXPECT_MEMORY_ALLOCATION_CHANGE(80, 0);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 84);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_MEMORY_ALLOCATION_CHANGE(84, 0);
}

TEST_F(TextureTest, UnusedMips) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Set level zero to large size.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  // Set level zero to large smaller (levels unused mips)
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  // Set an unused level to some size
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 4, GL_RGBA, 16, 16,
                         1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(16, 16));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureTest, NPOT2D) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Check Setting level 0 to NPOT
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 5));
  EXPECT_TRUE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
  // Change it to POT.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
}

TEST_F(TextureTest, NPOT2DNPOTOK) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Check Setting level 0 to NPOT
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 5));
  EXPECT_TRUE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager.CanGenerateMipmaps(texture_ref));
  EXPECT_FALSE(manager.CanRender(texture_ref));
  manager.MarkMipmapsGenerated(texture_ref);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager.CanRender(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, POTCubeMap) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_CUBE_MAP);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), texture->target());
  // Check Setting level 0 each face to POT
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));

  // Make mips.
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));

  // Change a mip.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 3,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  // Make mips.
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture));
}

TEST_F(TextureTest, POTCubeMapWithoutMipmap) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_CUBE_MAP);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_NEAREST, GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);

  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), texture->target());
  // Check Setting level 0 each face to POT
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_FALSE(manager_->CanRender(texture_ref_.get()));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0,
                         GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(4, 4));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  EXPECT_TRUE(manager_->CanRender(texture_ref_.get()));
}

TEST_F(TextureTest, GetLevelSize) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_3D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_3D, 1, GL_RGBA, 4, 5, 6,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 5));
  GLsizei width = -1;
  GLsizei height = -1;
  GLsizei depth = -1;
  Texture* texture = texture_ref_->texture();
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_3D, -1, &width, &height, &depth));
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_3D, 1000, &width, &height, &depth));
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_3D, 0, &width, &height, &depth));
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_3D, 1, &width, &height, &depth));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
  EXPECT_EQ(6, depth);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_3D, 1, &width, &height, &depth));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
  EXPECT_EQ(6, depth);
}

TEST_F(TextureTest, GetLevelSizeTexture2DArray) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D_ARRAY);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4,
                         4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4));
  GLsizei width = -1;
  GLsizei height = -1;
  GLsizei depth = -1;
  Texture* texture = texture_ref_->texture();
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_FALSE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, -1,
                                     &width, &height, &depth));
  EXPECT_FALSE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 1000,
                                     &width, &height, &depth));
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 0,
                                    &width, &height, &depth));
  EXPECT_EQ(4, width);
  EXPECT_EQ(4, height);
  EXPECT_EQ(2, depth);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 1,
                                    &width, &height, &depth));
  EXPECT_EQ(2, width);
  EXPECT_EQ(2, height);
  EXPECT_EQ(2, depth);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 2,
                                    &width, &height, &depth));
  EXPECT_EQ(1, width);
  EXPECT_EQ(1, height);
  EXPECT_EQ(2, depth);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 0,
                                    &width, &height, &depth));
  EXPECT_EQ(4, width);
  EXPECT_EQ(4, height);
  EXPECT_EQ(2, depth);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 1,
                                    &width, &height, &depth));
  EXPECT_EQ(2, width);
  EXPECT_EQ(2, height);
  EXPECT_EQ(2, depth);
  EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D_ARRAY, 2,
                                    &width, &height, &depth));
  EXPECT_EQ(1, width);
  EXPECT_EQ(1, height);
  EXPECT_EQ(2, depth);
}

TEST_F(TextureTest, GetLevelType) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 5));
  GLenum type = 0;
  GLenum format = 0;
  Texture* texture = texture_ref_->texture();
  EXPECT_FALSE(texture->GetLevelType(GL_TEXTURE_2D, -1, &type, &format));
  EXPECT_FALSE(texture->GetLevelType(GL_TEXTURE_2D, 1000, &type, &format));
  EXPECT_FALSE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &format));
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
}

TEST_F(TextureTest, ValidForTexture) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 6,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 5));
  // Check bad face.
  Texture* texture = texture_ref_->texture();
  EXPECT_FALSE(texture->ValidForTexture(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 1, 0, 0, 0, 4, 5, 6));
  // Check bad level.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 0, 0, 0, 0, 4, 5, 6));
  // Check bad xoffset.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, -1, 0, 0, 4, 5, 6));
  // Check bad xoffset + width > width.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 1, 0, 0, 4, 5, 6));
  // Check bad yoffset.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, -1, 0, 4, 5, 6));
  // Check bad yoffset + height > height.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 1, 0, 4, 5, 6));
  // Check bad zoffset.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, -1, 4, 5, 6));
  // Check bad zoffset + depth > depth.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 1, 4, 5, 6));
  // Check bad width.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 0, 5, 5, 6));
  // Check bad height.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 0, 4, 6, 6));
  // Check bad depth.
  EXPECT_FALSE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 0, 4, 5, 7));
  // Check valid full size
  EXPECT_TRUE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 0, 4, 5, 6));
  // Check valid particial size.
  EXPECT_TRUE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 1, 1, 1, 2, 3, 4));
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture->ValidForTexture(GL_TEXTURE_2D, 1, 0, 0, 0, 4, 5, 6));
}

TEST_F(TextureTest, FloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                       GL_RGBA, GL_FLOAT, gfx::Rect(1, 1));
  EXPECT_FALSE(manager.CanRender(texture_ref));
  TestHelper::SetTexParameteriWithExpectations(
      gl_.get(), error_state_.get(), &manager,
      texture_ref, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_FALSE(manager.CanRender(texture_ref));
  TestHelper::SetTexParameteriWithExpectations(
      gl_.get(), error_state_.get(), &manager, texture_ref,
      GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST, GL_NO_ERROR);
  EXPECT_TRUE(manager.CanRender(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, FloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float GL_OES_texture_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                       GL_RGBA, GL_FLOAT, gfx::Rect(1, 1));
  EXPECT_TRUE(manager.CanRender(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, HalfFloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                       GL_RGBA, GL_HALF_FLOAT_OES, gfx::Rect(1, 1));
  EXPECT_FALSE(manager.CanRender(texture_ref));
  TestHelper::SetTexParameteriWithExpectations(
      gl_.get(), error_state_.get(), &manager,
      texture_ref, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_FALSE(manager.CanRender(texture_ref));
  TestHelper::SetTexParameteriWithExpectations(
      gl_.get(), error_state_.get(), &manager, texture_ref,
      GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST, GL_NO_ERROR);
  EXPECT_TRUE(manager.CanRender(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, HalfFloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                       GL_RGBA, GL_HALF_FLOAT_OES, gfx::Rect(1, 1));
  EXPECT_TRUE(manager.CanRender(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, EGLImageExternal) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_EGL_image_external");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_EXTERNAL_OES);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), texture->target());
  EXPECT_FALSE(manager.CanGenerateMipmaps(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, DepthTexture) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_ANGLE_depth_texture");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->InitializeForTesting();
  TextureManager manager(nullptr, feature_info.get(), kMaxTextureSize,
                         kMaxCubeMapTextureSize, kMaxRectangleTextureSize,
                         kMax3DTextureSize, kMaxArrayTextureLayers,
                         kUseDefaultTextures, nullptr, &discardable_manager_);
  manager.CreateTexture(kClient1Id, kService1Id);
  TextureRef* texture_ref = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture_ref != nullptr);
  manager.SetTarget(texture_ref, GL_TEXTURE_2D);
  manager.SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4,
                       1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, gfx::Rect());
  EXPECT_FALSE(manager.CanGenerateMipmaps(texture_ref));
  manager.MarkContextLost();
  manager.Destroy();
}

TEST_F(TextureTest, SafeUnsafe) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  static const GLuint kClient3Id = 3;
  static const GLuint kService3Id = 13;
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(0, texture->num_uncleared_mips());
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref_.get(), GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref_.get(), GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref_.get(), GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref_.get(), GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture->num_uncleared_mips());
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());

  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<TextureRef> texture_ref2(
      manager_->GetTexture(kClient2Id));
  ASSERT_TRUE(texture_ref2.get() != nullptr);
  manager_->SetTarget(texture_ref2.get(), GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  Texture* texture2 = texture_ref2->texture();
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref2.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(8, 8));
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref2.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture2->num_uncleared_mips());

  manager_->CreateTexture(kClient3Id, kService3Id);
  scoped_refptr<TextureRef> texture_ref3(
      manager_->GetTexture(kClient3Id));
  ASSERT_TRUE(texture_ref3.get() != nullptr);
  manager_->SetTarget(texture_ref3.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref3.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  Texture* texture3 = texture_ref3->texture();
  EXPECT_EQ(1, texture3->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref2.get(), GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelCleared(texture_ref3.get(), GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture3->num_uncleared_mips());

  manager_->SetLevelInfo(texture_ref2.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(1, 1, 1, 1));
  manager_->SetLevelInfo(texture_ref3.get(), GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 4, 4, 4));
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture2->num_uncleared_mips());
  EXPECT_EQ(1, texture3->num_uncleared_mips());
  manager_->RemoveTexture(kClient3Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  manager_->RemoveTexture(kClient2Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture_ref2 = nullptr;
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService3Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture_ref3 = nullptr;
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
}

TEST_F(TextureTest, ClearTexture) {
  EXPECT_CALL(*decoder_, ClearLevel(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  // The code path taken when IsCompressedTextureFormat returns true
  // is covered best by the WebGL 2.0 conformance tests.
  EXPECT_CALL(*decoder_, IsCompressedTextureFormat(_))
      .WillRepeatedly(Return(false));
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  Texture* texture = texture_ref_->texture();
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture->num_uncleared_mips());
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  manager_->ClearRenderableLevels(decoder_.get(), texture_ref_.get());
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 3));
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture->num_uncleared_mips());
  manager_->ClearTextureLevel(
      decoder_.get(), texture_ref_.get(), GL_TEXTURE_2D, 0);
  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture->num_uncleared_mips());
  manager_->ClearTextureLevel(
      decoder_.get(), texture_ref_.get(), GL_TEXTURE_2D, 1);
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture->num_uncleared_mips());
}

TEST_F(TextureTest, UseDeletedTexture) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  // Make the default texture renderable
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  // Make a new texture
  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<TextureRef> texture_ref(
      manager_->GetTexture(kClient2Id));
  manager_->SetTarget(texture_ref.get(), GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->CanRender(texture_ref.get()));
  // Remove it.
  manager_->RemoveTexture(kClient2Id);
  EXPECT_FALSE(manager_->CanRender(texture_ref.get()));
  // Check that we can still manipulate it and it effects the manager.
  manager_->SetLevelInfo(texture_ref.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_TRUE(manager_->CanRender(texture_ref.get()));
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture_ref = nullptr;
}

TEST_F(TextureTest, GetLevelImage) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  Texture* texture = texture_ref_->texture();
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 1) == nullptr);
  // Set image.
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  manager_->SetLevelImage(texture_ref_.get(), GL_TEXTURE_2D, 1, image.get(),
                          Texture::BOUND);
  EXPECT_FALSE(texture->GetLevelImage(GL_TEXTURE_2D, 1) == nullptr);
  EXPECT_TRUE(texture->GetLevelStreamTextureImage(GL_TEXTURE_2D, 1) == nullptr);
  // Remove it.
  manager_->SetLevelImage(texture_ref_.get(), GL_TEXTURE_2D, 1, nullptr,
                          Texture::UNBOUND);
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 1) == nullptr);
  manager_->SetLevelImage(texture_ref_.get(), GL_TEXTURE_2D, 1, image.get(),
                          Texture::UNBOUND);
  // Image should be reset when SetLevelInfo is called.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 1) == nullptr);
  EXPECT_TRUE(texture->GetLevelStreamTextureImage(GL_TEXTURE_2D, 1) == nullptr);
}

TEST_F(TextureTest, GetLevelStreamTextureImage) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                         GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(2, 2));
  Texture* texture = texture_ref_->texture();

  // Set image.
  scoped_refptr<GLStreamTextureImage> image(new GLStreamTextureImageStub);
  manager_->SetLevelStreamTextureImage(texture_ref_.get(),
                                       GL_TEXTURE_EXTERNAL_OES, 0, image.get(),
                                       Texture::BOUND, 0);
  EXPECT_FALSE(texture->GetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0) == nullptr);
  EXPECT_FALSE(texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES,
                                                   0) == nullptr);

  // Replace it as a normal image.
  scoped_refptr<gl::GLImage> image2(new gl::GLImageStub);
  manager_->SetLevelImage(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                          image2.get(), Texture::BOUND);
  EXPECT_FALSE(texture->GetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0) == nullptr);
  EXPECT_TRUE(texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES, 0) ==
              nullptr);

  // Image should be reset when SetLevelInfo is called.
  manager_->SetLevelStreamTextureImage(texture_ref_.get(),
                                       GL_TEXTURE_EXTERNAL_OES, 0, image.get(),
                                       Texture::UNBOUND, 0);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                         GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(2, 2));
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_EXTERNAL_OES, 0) == nullptr);
  EXPECT_TRUE(texture->GetLevelStreamTextureImage(GL_TEXTURE_EXTERNAL_OES, 0) ==
              nullptr);
}

TEST_F(TextureTest, SetLevelImageState) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  Texture* texture = texture_ref_->texture();
  // Set image, initially BOUND.
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  manager_->SetLevelImage(texture_ref_.get(), GL_TEXTURE_2D, 0, image.get(),
                          Texture::BOUND);
  Texture::ImageState state;
  texture->GetLevelImage(GL_TEXTURE_2D, 0, &state);
  EXPECT_EQ(state, Texture::BOUND);
  // Change the state.
  texture->SetLevelImageState(GL_TEXTURE_2D, 0, Texture::COPIED);
  texture->GetLevelImage(GL_TEXTURE_2D, 0, &state);
  EXPECT_EQ(state, Texture::COPIED);
}

TEST_F(TextureTest, SetStreamTextureImageServiceID) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0,
                         GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         gfx::Rect(2, 2));
  Texture* texture = texture_ref_->texture();

  GLuint owned_service_id = TextureTestHelper::owned_service_id(texture);
  GLuint service_id = texture->service_id();
  // Initially, the texture should use the same service id that it owns.
  EXPECT_EQ(owned_service_id, service_id);

  // Override the service_id.
  GLuint stream_texture_service_id = service_id + 1;
  scoped_refptr<GLStreamTextureImage> image(new GLStreamTextureImageStub);
  manager_->SetLevelStreamTextureImage(
      texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0, image.get(),
      Texture::BOUND, stream_texture_service_id);

  // Make sure that service_id() changed but owned_service_id() didn't.
  EXPECT_EQ(stream_texture_service_id, texture->service_id());
  EXPECT_EQ(owned_service_id, TextureTestHelper::owned_service_id(texture));

  // Undo the override.
  manager_->SetLevelStreamTextureImage(texture_ref_.get(),
                                       GL_TEXTURE_EXTERNAL_OES, 0, image.get(),
                                       Texture::BOUND, 0);

  // The service IDs should be back as they were.
  EXPECT_EQ(service_id, texture->service_id());
  EXPECT_EQ(owned_service_id, TextureTestHelper::owned_service_id(texture));

  // Override again, so that we can check delete behavior.
  manager_->SetLevelStreamTextureImage(
      texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES, 0, image.get(),
      Texture::BOUND, stream_texture_service_id);

  // Remove the Texture.  It should delete the texture id that it owns, even
  // though it is overridden.
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(owned_service_id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->RemoveTexture(kClient1Id);
  texture_ref_ = nullptr;
}

namespace {

bool InSet(std::set<std::string>* string_set, const std::string& str) {
  std::pair<std::set<std::string>::iterator, bool> result =
      string_set->insert(str);
  return !result.second;
}

}  // anonymous namespace

TEST_F(TextureTest, AddToSignature) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  std::string signature1;
  std::string signature2;
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature1);

  std::set<std::string> string_set;
  EXPECT_FALSE(InSet(&string_set, signature1));

  // check changing 1 thing makes a different signature.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 4, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(4, 2));
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // check putting it back makes the same signature.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(2, 2));
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check setting cleared status does not change signature.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check changing other settings changes signature.
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 4, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 2,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         1, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGB, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_FLOAT, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // put it back
  manager_->SetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // check changing parameters changes signature.
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MIN_FILTER, GL_NEAREST, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(texture_ref_.get(),
               GL_TEXTURE_MIN_FILTER,
               GL_NEAREST_MIPMAP_LINEAR,
               GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(
      texture_ref_.get(), GL_TEXTURE_MAG_FILTER, GL_LINEAR, GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(texture_ref_.get(), GL_TEXTURE_WRAP_S, GL_REPEAT, GL_NO_ERROR);
  SetParameter(
      texture_ref_.get(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // Check putting it back genenerates the same signature
  SetParameter(texture_ref_.get(), GL_TEXTURE_WRAP_T, GL_REPEAT, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_ref_.get(), GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check the set was acutally getting different signatures.
  EXPECT_EQ(11u, string_set.size());
}

class ProduceConsumeTextureTest : public TextureTest,
                                  public ::testing::WithParamInterface<GLenum> {
 public:
  void SetUp() override {
    TextureTest::SetUpBase(nullptr, "GL_OES_EGL_image_external");
    manager_->CreateTexture(kClient2Id, kService2Id);
    texture2_ = manager_->GetTexture(kClient2Id);

    EXPECT_CALL(*decoder_.get(), GetErrorState())
      .WillRepeatedly(Return(error_state_.get()));
  }

  void TearDown() override {
    if (texture2_.get()) {
      // If it's not in the manager then setting texture2_ to NULL will
      // delete the texture.
      if (!texture2_->client_id()) {
        // Check that it gets deleted when the last reference is released.
        EXPECT_CALL(
            *gl_,
            DeleteTextures(1, ::testing::Pointee(texture2_->service_id())))
            .Times(1).RetiresOnSaturation();
      }
      texture2_ = nullptr;
    }
    TextureTest::TearDown();
  }

 protected:
  struct LevelInfo {
    LevelInfo(GLenum target,
              GLenum format,
              GLsizei width,
              GLsizei height,
              GLsizei depth,
              GLint border,
              GLenum type,
              const gfx::Rect& cleared_rect)
        : target(target),
          format(format),
          width(width),
          height(height),
          depth(depth),
          border(border),
          type(type),
          cleared_rect(cleared_rect) {}

    LevelInfo()
        : target(0),
          format(0),
          width(-1),
          height(-1),
          depth(1),
          border(0),
          type(0) {}

    bool operator==(const LevelInfo& other) const {
      return target == other.target && format == other.format &&
             width == other.width && height == other.height &&
             depth == other.depth && border == other.border &&
             type == other.type && cleared_rect == other.cleared_rect;
    }

    GLenum target;
    GLenum format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum type;
    gfx::Rect cleared_rect;
  };

  void SetLevelInfo(TextureRef* texture_ref,
                    GLint level,
                    const LevelInfo& info) {
    manager_->SetLevelInfo(texture_ref, info.target, level, info.format,
                           info.width, info.height, info.depth, info.border,
                           info.format, info.type, info.cleared_rect);
  }

  static LevelInfo GetLevelInfo(const TextureRef* texture_ref,
                                GLint target,
                                GLint level) {
    const Texture* texture = texture_ref->texture();
    LevelInfo info;
    info.target = target;
    EXPECT_TRUE(texture->GetLevelSize(target, level, &info.width,
                                      &info.height, &info.depth));
    EXPECT_TRUE(texture->GetLevelType(target, level, &info.type,
                                      &info.format));
    info.cleared_rect = texture->GetLevelClearedRect(target, level);
    return info;
  }

  Texture* Produce(TextureRef* texture_ref) { return texture_ref->texture(); }

  void Consume(GLuint client_id, Texture* texture) {
    EXPECT_TRUE(manager_->Consume(client_id, texture));
  }

  scoped_refptr<TextureRef> texture2_;

 private:
  static const GLuint kClient2Id;
  static const GLuint kService2Id;
};

const GLuint ProduceConsumeTextureTest::kClient2Id = 2;
const GLuint ProduceConsumeTextureTest::kService2Id = 12;

TEST_F(ProduceConsumeTextureTest, ProduceConsume2D) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_2D);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  LevelInfo level0(GL_TEXTURE_2D, GL_RGBA, 4, 4, 1, 0, GL_UNSIGNED_BYTE,
                   gfx::Rect(4, 4));
  SetLevelInfo(texture_ref_.get(), 0, level0);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture_ref_.get());
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  LevelInfo level1 = GetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 1);
  LevelInfo level2 = GetLevelInfo(texture_ref_.get(), GL_TEXTURE_2D, 2);
  Texture* produced_texture = Produce(texture_ref_.get());
  EXPECT_EQ(produced_texture, texture);

  // Make this texture bigger with more levels, and make sure they get
  // clobbered correctly during Consume().
  manager_->SetTarget(texture2_.get(), GL_TEXTURE_2D);
  SetLevelInfo(texture2_.get(), 0, LevelInfo(GL_TEXTURE_2D, GL_RGBA, 16, 16, 1,
                                             0, GL_UNSIGNED_BYTE, gfx::Rect()));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_ref_.get()));
  manager_->MarkMipmapsGenerated(texture2_.get());
  texture = texture2_->texture();
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_EQ(1024U + 256U + 64U + 16U + 4U, texture->estimated_size());

  GLuint client_id = texture2_->client_id();
  manager_->RemoveTexture(client_id);
  Consume(client_id, produced_texture);
  scoped_refptr<TextureRef> restored_texture = manager_->GetTexture(client_id);
  EXPECT_EQ(produced_texture, restored_texture->texture());
  EXPECT_EQ(level0, GetLevelInfo(restored_texture.get(), GL_TEXTURE_2D, 0));
  EXPECT_EQ(level1, GetLevelInfo(restored_texture.get(), GL_TEXTURE_2D, 1));
  EXPECT_EQ(level2, GetLevelInfo(restored_texture.get(), GL_TEXTURE_2D, 2));
  texture = restored_texture->texture();
  EXPECT_EQ(64U + 16U + 4U, texture->estimated_size());
  GLint w, h;
  EXPECT_FALSE(texture->GetLevelSize(GL_TEXTURE_2D, 3, &w, &h, nullptr));

  // However the old texture ref still exists if it was referenced somewhere.
  EXPECT_EQ(1024U + 256U + 64U + 16U + 4U,
            texture2_->texture()->estimated_size());
}

TEST_F(ProduceConsumeTextureTest, ProduceConsumeClearRectangle) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_RECTANGLE_ARB);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_RECTANGLE_ARB), texture->target());
  LevelInfo level0(GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, 1, 1, 1, 0,
                   GL_UNSIGNED_BYTE, gfx::Rect());
  SetLevelInfo(texture_ref_.get(), 0, level0);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  Texture* produced_texture = Produce(texture_ref_.get());
  EXPECT_EQ(produced_texture, texture);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_RECTANGLE_ARB),
            produced_texture->target());

  GLuint client_id = texture2_->client_id();
  manager_->RemoveTexture(client_id);
  Consume(client_id, produced_texture);
  scoped_refptr<TextureRef> restored_texture = manager_->GetTexture(client_id);
  EXPECT_EQ(produced_texture, restored_texture->texture());

  // See if we can clear the previously uncleared level now.
  EXPECT_EQ(level0,
            GetLevelInfo(restored_texture.get(), GL_TEXTURE_RECTANGLE_ARB, 0));
  EXPECT_CALL(*decoder_, ClearLevel(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  // The code path taken when IsCompressedTextureFormat returns true
  // is covered best by the WebGL 2.0 conformance tests.
  EXPECT_CALL(*decoder_, IsCompressedTextureFormat(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*decoder_.get(), GetFeatureInfo())
     .WillRepeatedly(Return(feature_info_.get()));
  EXPECT_TRUE(manager_->ClearTextureLevel(
      decoder_.get(), restored_texture.get(), GL_TEXTURE_RECTANGLE_ARB, 0));
}

TEST_F(ProduceConsumeTextureTest, ProduceConsumeExternal) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_EXTERNAL_OES);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), texture->target());
  LevelInfo level0(GL_TEXTURE_EXTERNAL_OES, GL_RGBA, 1, 1, 1, 0,
                   GL_UNSIGNED_BYTE, gfx::Rect());
  SetLevelInfo(texture_ref_.get(), 0, level0);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  Texture* produced_texture = Produce(texture_ref_.get());
  EXPECT_EQ(produced_texture, texture);

  GLuint client_id = texture2_->client_id();
  manager_->RemoveTexture(client_id);
  Consume(client_id, produced_texture);
  scoped_refptr<TextureRef> restored_texture = manager_->GetTexture(client_id);
  EXPECT_EQ(produced_texture, restored_texture->texture());
  EXPECT_EQ(level0,
            GetLevelInfo(restored_texture.get(), GL_TEXTURE_EXTERNAL_OES, 0));
}

TEST_P(ProduceConsumeTextureTest, ProduceConsumeTextureWithImage) {
  GLenum target = GetParam();
  manager_->SetTarget(texture_ref_.get(), target);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(target), texture->target());
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  manager_->SetLevelInfo(texture_ref_.get(), target, 0, GL_RGBA, 0, 0, 1, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  manager_->SetLevelImage(texture_ref_.get(), target, 0, image.get(),
                          Texture::BOUND);
  GLuint service_id = texture->service_id();
  Texture* produced_texture = Produce(texture_ref_.get());

  GLuint client_id = texture2_->client_id();
  manager_->RemoveTexture(client_id);
  Consume(client_id, produced_texture);
  scoped_refptr<TextureRef> restored_texture = manager_->GetTexture(client_id);
  EXPECT_EQ(produced_texture, restored_texture->texture());
  EXPECT_EQ(service_id, restored_texture->service_id());
  EXPECT_EQ(image.get(), restored_texture->texture()->GetLevelImage(target, 0));
}

static const GLenum kTextureTargets[] = {GL_TEXTURE_2D, GL_TEXTURE_EXTERNAL_OES,
                                         GL_TEXTURE_RECTANGLE_ARB, };

INSTANTIATE_TEST_SUITE_P(Target,
                         ProduceConsumeTextureTest,
                         ::testing::ValuesIn(kTextureTargets));

TEST_F(ProduceConsumeTextureTest, ProduceConsumeCube) {
  manager_->SetTarget(texture_ref_.get(), GL_TEXTURE_CUBE_MAP);
  Texture* texture = texture_ref_->texture();
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), texture->target());
  LevelInfo face0(GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_RGBA, 1, 1, 1, 0,
                  GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  LevelInfo face5(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GL_RGBA, 3, 3, 1, 0,
                  GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  SetLevelInfo(texture_ref_.get(), 0, face0);
  SetLevelInfo(texture_ref_.get(), 0, face5);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  Texture* produced_texture = Produce(texture_ref_.get());
  EXPECT_EQ(produced_texture, texture);

  GLuint client_id = texture2_->client_id();
  manager_->RemoveTexture(client_id);
  Consume(client_id, produced_texture);
  scoped_refptr<TextureRef> restored_texture = manager_->GetTexture(client_id);
  EXPECT_EQ(produced_texture, restored_texture->texture());
  EXPECT_EQ(
      face0,
      GetLevelInfo(restored_texture.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0));
  EXPECT_EQ(
      face5,
      GetLevelInfo(restored_texture.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0));
}

class CountingMemoryTracker : public MemoryTracker {
 public:
  CountingMemoryTracker() {
    current_size_ = 0;
  }
  ~CountingMemoryTracker() override = default;

  void TrackMemoryAllocatedChange(uint64_t delta) override {
    current_size_ += delta;
  }

  uint64_t GetSize() const override { return current_size_; }

  uint64_t ClientTracingId() const override { return 0; }

  int ClientId() const override { return 0; }

  uint64_t ContextGroupTracingId() const override { return 0; }

 private:
  uint64_t current_size_;
  DISALLOW_COPY_AND_ASSIGN(CountingMemoryTracker);
};

class SharedTextureTest : public GpuServiceTest {
 public:
  static const bool kUseDefaultTextures = false;

  SharedTextureTest() : feature_info_(new FeatureInfo()) {}

  ~SharedTextureTest() override = default;

  void SetUp() override {
    GpuServiceTest::SetUp();
    texture_manager1_.reset(new TextureManager(
        &memory_tracker1_, feature_info_.get(),
        TextureManagerTest::kMaxTextureSize,
        TextureManagerTest::kMaxCubeMapTextureSize,
        TextureManagerTest::kMaxRectangleTextureSize,
        TextureManagerTest::kMax3DTextureSize,
        TextureManagerTest::kMaxArrayTextureLayers, kUseDefaultTextures,
        nullptr, &discardable_manager_));
    texture_manager2_.reset(new TextureManager(
        &memory_tracker2_, feature_info_.get(),
        TextureManagerTest::kMaxTextureSize,
        TextureManagerTest::kMaxCubeMapTextureSize,
        TextureManagerTest::kMaxRectangleTextureSize,
        TextureManagerTest::kMax3DTextureSize,
        TextureManagerTest::kMaxArrayTextureLayers, kUseDefaultTextures,
        nullptr, &discardable_manager_));
    SetupFeatureInfo("", "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);
    TestHelper::SetupTextureManagerInitExpectations(
        gl_.get(), false, false, false, {}, kUseDefaultTextures);
    texture_manager1_->Initialize();
    TestHelper::SetupTextureManagerInitExpectations(
        gl_.get(), false, false, false, {}, kUseDefaultTextures);
    texture_manager2_->Initialize();
  }

  void TearDown() override {
    texture_manager2_->MarkContextLost();
    texture_manager2_->Destroy();
    texture_manager2_.reset();
    texture_manager1_->MarkContextLost();
    texture_manager1_->Destroy();
    texture_manager1_.reset();
    GpuServiceTest::TearDown();
  }

 protected:
  void SetupFeatureInfo(const char* gl_extensions,
                        const char* gl_version,
                        ContextType context_type) {
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), gl_extensions, "", gl_version, context_type);
    feature_info_->InitializeForTesting(context_type);
    ASSERT_TRUE(feature_info_->context_type() == context_type);
    if (feature_info_->IsWebGL2OrES3Context()) {
      EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_COLOR_ATTACHMENTS, _))
          .WillOnce(SetArgPointee<1>(8))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_DRAW_BUFFERS, _))
          .WillOnce(SetArgPointee<1>(8))
          .RetiresOnSaturation();
      feature_info_->EnableES3Validators();
    }
  }

  scoped_refptr<FeatureInfo> feature_info_;
  ServiceDiscardableManager discardable_manager_;
  CountingMemoryTracker memory_tracker1_;
  std::unique_ptr<TextureManager> texture_manager1_;
  CountingMemoryTracker memory_tracker2_;
  std::unique_ptr<TextureManager> texture_manager2_;
};

TEST_F(SharedTextureTest, DeleteTextures) {
  scoped_refptr<TextureRef> ref1 = texture_manager1_->CreateTexture(10, 10);
  scoped_refptr<TextureRef> ref2 =
      texture_manager2_->Consume(20, ref1->texture());
  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(0);
  ref1 = nullptr;
  texture_manager1_->RemoveTexture(10);
  testing::Mock::VerifyAndClearExpectations(gl_.get());

  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  ref2 = nullptr;
  texture_manager2_->RemoveTexture(20);
  testing::Mock::VerifyAndClearExpectations(gl_.get());
}

TEST_F(SharedTextureTest, TextureSafetyAccounting) {
  EXPECT_FALSE(texture_manager1_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager1_->HaveUnclearedMips());
  EXPECT_FALSE(texture_manager2_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager2_->HaveUnclearedMips());

  // Newly created texture is renderable.
  scoped_refptr<TextureRef> ref1 = texture_manager1_->CreateTexture(10, 10);
  EXPECT_FALSE(texture_manager1_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager1_->HaveUnclearedMips());

  // Associate new texture ref to other texture manager, should account for it
  // too.
  scoped_refptr<TextureRef> ref2 =
      texture_manager2_->Consume(20, ref1->texture());
  EXPECT_FALSE(texture_manager2_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager2_->HaveUnclearedMips());

  // Make texture renderable but uncleared on one texture manager, should affect
  // other one.
  texture_manager1_->SetTarget(ref1.get(), GL_TEXTURE_2D);
  EXPECT_FALSE(texture_manager1_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager1_->HaveUnclearedMips());
  EXPECT_FALSE(texture_manager2_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager2_->HaveUnclearedMips());

  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());
  EXPECT_TRUE(texture_manager1_->HaveUnsafeTextures());
  EXPECT_TRUE(texture_manager1_->HaveUnclearedMips());
  EXPECT_TRUE(texture_manager2_->HaveUnsafeTextures());
  EXPECT_TRUE(texture_manager2_->HaveUnclearedMips());

  // Make texture cleared on one texture manager, should affect other one.
  texture_manager1_->SetLevelCleared(ref1.get(), GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(texture_manager1_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager1_->HaveUnclearedMips());
  EXPECT_FALSE(texture_manager2_->HaveUnsafeTextures());
  EXPECT_FALSE(texture_manager2_->HaveUnclearedMips());

  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  texture_manager1_->RemoveTexture(10);
  texture_manager2_->RemoveTexture(20);
}

TEST_F(SharedTextureTest, FBOCompletenessCheck) {
  const GLenum kCompleteValue = GL_FRAMEBUFFER_COMPLETE;
  FramebufferManager framebuffer_manager1(1, 1, nullptr);
  texture_manager1_->AddFramebufferManager(&framebuffer_manager1);
  FramebufferManager framebuffer_manager2(1, 1, nullptr);
  texture_manager2_->AddFramebufferManager(&framebuffer_manager2);

  scoped_refptr<TextureRef> ref1 = texture_manager1_->CreateTexture(10, 10);
  framebuffer_manager1.CreateFramebuffer(10, 10);
  scoped_refptr<Framebuffer> framebuffer1 =
      framebuffer_manager1.GetFramebuffer(10);
  framebuffer1->AttachTexture(
      GL_COLOR_ATTACHMENT0, ref1.get(), GL_TEXTURE_2D, 0, 0);
  EXPECT_FALSE(framebuffer_manager1.IsComplete(framebuffer1.get()));
  EXPECT_NE(kCompleteValue,
            framebuffer1->IsPossiblyComplete(feature_info_.get()));

  // Make FBO complete in manager 1.
  texture_manager1_->SetTarget(ref1.get(), GL_TEXTURE_2D);
  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                  gfx::Rect(1, 1));
  EXPECT_EQ(kCompleteValue,
            framebuffer1->IsPossiblyComplete(feature_info_.get()));
  framebuffer_manager1.MarkAsComplete(framebuffer1.get());
  EXPECT_TRUE(framebuffer_manager1.IsComplete(framebuffer1.get()));

  // Share texture with manager 2.
  scoped_refptr<TextureRef> ref2 =
      texture_manager2_->Consume(20, ref1->texture());
  framebuffer_manager2.CreateFramebuffer(20, 20);
  scoped_refptr<Framebuffer> framebuffer2 =
      framebuffer_manager2.GetFramebuffer(20);
  framebuffer2->AttachTexture(
      GL_COLOR_ATTACHMENT0, ref2.get(), GL_TEXTURE_2D, 0, 0);
  EXPECT_FALSE(framebuffer_manager2.IsComplete(framebuffer2.get()));
  EXPECT_EQ(kCompleteValue,
            framebuffer2->IsPossiblyComplete(feature_info_.get()));
  framebuffer_manager2.MarkAsComplete(framebuffer2.get());
  EXPECT_TRUE(framebuffer_manager2.IsComplete(framebuffer2.get()));

  // Change level for texture, both FBOs should be marked incomplete
  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                  gfx::Rect(1, 1));
  EXPECT_FALSE(framebuffer_manager1.IsComplete(framebuffer1.get()));
  EXPECT_EQ(kCompleteValue,
            framebuffer1->IsPossiblyComplete(feature_info_.get()));
  framebuffer_manager1.MarkAsComplete(framebuffer1.get());
  EXPECT_TRUE(framebuffer_manager1.IsComplete(framebuffer1.get()));
  EXPECT_FALSE(framebuffer_manager2.IsComplete(framebuffer2.get()));
  EXPECT_EQ(kCompleteValue,
            framebuffer2->IsPossiblyComplete(feature_info_.get()));
  framebuffer_manager2.MarkAsComplete(framebuffer2.get());
  EXPECT_TRUE(framebuffer_manager2.IsComplete(framebuffer2.get()));

  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, _))
      .Times(2)
      .RetiresOnSaturation();
  framebuffer_manager1.RemoveFramebuffer(10);
  framebuffer_manager2.RemoveFramebuffer(20);
  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  texture_manager1_->RemoveTexture(10);
  texture_manager2_->RemoveTexture(20);
}

TEST_F(SharedTextureTest, Memory) {
  size_t initial_memory1 = memory_tracker1_.GetSize();
  size_t initial_memory2 = memory_tracker2_.GetSize();

  // Newly created texture is unrenderable.
  scoped_refptr<TextureRef> ref1 = texture_manager1_->CreateTexture(10, 10);
  texture_manager1_->SetTarget(ref1.get(), GL_TEXTURE_2D);
  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 0, GL_RGBA, 10, 10,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect());

  EXPECT_LT(0u, ref1->texture()->estimated_size());
  EXPECT_EQ(initial_memory1 + ref1->texture()->estimated_size(),
            memory_tracker1_.GetSize());

  // Associate new texture ref to other texture manager, it doesn't account for
  // the texture memory, the first memory tracker still has it.
  scoped_refptr<TextureRef> ref2 =
      texture_manager2_->Consume(20, ref1->texture());
  EXPECT_EQ(initial_memory1 + ref1->texture()->estimated_size(),
            memory_tracker1_.GetSize());
  EXPECT_EQ(initial_memory2, memory_tracker2_.GetSize());

  // Delete the texture, memory should go to the remaining tracker.
  texture_manager1_->RemoveTexture(10);
  ref1 = nullptr;
  EXPECT_EQ(initial_memory1, memory_tracker1_.GetSize());
  EXPECT_EQ(initial_memory2 + ref2->texture()->estimated_size(),
            memory_tracker2_.GetSize());

  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  ref2 = nullptr;
  texture_manager2_->RemoveTexture(20);
  EXPECT_EQ(initial_memory2, memory_tracker2_.GetSize());
}

TEST_F(SharedTextureTest, Images) {
  scoped_refptr<TextureRef> ref1 = texture_manager1_->CreateTexture(10, 10);
  scoped_refptr<TextureRef> ref2 =
      texture_manager2_->Consume(20, ref1->texture());

  texture_manager1_->SetTarget(ref1.get(), GL_TEXTURE_2D);
  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                  gfx::Rect(2, 2));
  EXPECT_FALSE(ref1->texture()->HasImages());
  EXPECT_FALSE(ref2->texture()->HasImages());
  EXPECT_FALSE(texture_manager1_->HaveImages());
  EXPECT_FALSE(texture_manager2_->HaveImages());
  scoped_refptr<gl::GLImage> image1(new gl::GLImageStub);
  texture_manager1_->SetLevelImage(ref1.get(), GL_TEXTURE_2D, 1, image1.get(),
                                   Texture::BOUND);
  EXPECT_TRUE(ref1->texture()->HasImages());
  EXPECT_TRUE(ref2->texture()->HasImages());
  EXPECT_TRUE(texture_manager1_->HaveImages());
  EXPECT_TRUE(texture_manager2_->HaveImages());
  scoped_refptr<gl::GLImage> image2(new gl::GLImageStub);
  texture_manager1_->SetLevelImage(ref1.get(), GL_TEXTURE_2D, 1, image2.get(),
                                   Texture::BOUND);
  EXPECT_TRUE(ref1->texture()->HasImages());
  EXPECT_TRUE(ref2->texture()->HasImages());
  EXPECT_TRUE(texture_manager1_->HaveImages());
  EXPECT_TRUE(texture_manager2_->HaveImages());
  texture_manager1_->SetLevelInfo(ref1.get(), GL_TEXTURE_2D, 1, GL_RGBA, 2, 2,
                                  1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                  gfx::Rect(2, 2));
  EXPECT_FALSE(ref1->texture()->HasImages());
  EXPECT_FALSE(ref2->texture()->HasImages());
  EXPECT_FALSE(texture_manager1_->HaveImages());
  EXPECT_FALSE(texture_manager1_->HaveImages());

  EXPECT_CALL(*gl_, DeleteTextures(1, _))
      .Times(1)
      .RetiresOnSaturation();
  texture_manager1_->RemoveTexture(10);
  texture_manager2_->RemoveTexture(20);
}


class TextureFormatTypeValidationTest : public TextureManagerTest {
 public:
  TextureFormatTypeValidationTest() = default;
  ~TextureFormatTypeValidationTest() override = default;

 protected:
  void ExpectValid(
      bool tex_image_call, GLenum format, GLenum type, GLint internal_format) {
    EXPECT_TRUE(manager_->ValidateTextureParameters(
        error_state_.get(), "", tex_image_call,
        format, type, internal_format, 0));
  }

  void ExpectInvalid(
      bool tex_image_call, GLenum format, GLenum type, GLint internal_format) {
    EXPECT_CALL(*error_state_,
                SetGLError(_, _, _, _, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_FALSE(manager_->ValidateTextureParameters(
        error_state_.get(), "", tex_image_call,
        format, type, internal_format, 0));
  }

  void ExpectInvalidEnum(
      bool tex_image_call, GLenum format, GLenum type, GLint internal_format) {
    EXPECT_CALL(*error_state_,
                SetGLErrorInvalidEnum(_, _, _, _, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_FALSE(manager_->ValidateTextureParameters(
        error_state_.get(), "", tex_image_call,
        format, type, internal_format, 0));
  }
};

TEST_F(TextureFormatTypeValidationTest, ES2Basic) {
  SetupFeatureInfo("", "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_ALPHA, GL_UNSIGNED_BYTE, GL_ALPHA);
  ExpectValid(true, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_UNSIGNED_BYTE, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA);

  ExpectInvalid(true, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_ALPHA);

  // float / half_float.
  ExpectInvalidEnum(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectInvalidEnum(true, GL_RGBA, GL_HALF_FLOAT_OES, GL_RGBA);

  // GL_EXT_bgra
  ExpectInvalidEnum(true, GL_BGRA_EXT, GL_UNSIGNED_BYTE, GL_BGRA_EXT);

  // depth / stencil
  ExpectInvalidEnum(
      true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT);
  ExpectInvalidEnum(
      true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL);

  // SRGB
  ExpectInvalidEnum(true, GL_SRGB_EXT, GL_UNSIGNED_BYTE, GL_SRGB_EXT);
  ExpectInvalidEnum(
      true, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE, GL_SRGB_ALPHA_EXT);

  // ES3
  ExpectInvalid(true, GL_RGB, GL_UNSIGNED_BYTE, GL_RGB8);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithExtTextureFormatBGRA8888) {
  SetupFeatureInfo("GL_EXT_texture_format_BGRA8888", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_BGRA_EXT, GL_UNSIGNED_BYTE, GL_BGRA_EXT);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithAppleTextureFormatBGRA8888) {
  SetupFeatureInfo("GL_APPLE_texture_format_BGRA8888", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_BGRA_EXT, GL_UNSIGNED_BYTE, GL_BGRA_EXT);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithArbDepth) {
  SetupFeatureInfo("GL_ARB_depth_texture", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT);
  ExpectInvalidEnum(
      true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithOesDepth) {
  SetupFeatureInfo("GL_OES_depth_texture", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT);
  ExpectInvalidEnum(
      true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithAngleDepth) {
  SetupFeatureInfo("GL_ANGLE_depth_texture", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT);
  ExpectInvalidEnum(
      true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithExtPackedDepthStencil) {
  SetupFeatureInfo("GL_EXT_packed_depth_stencil GL_ARB_depth_texture",
                   "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT);
  ExpectValid(true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithRGWithFloat) {
  SetupFeatureInfo(
      "GL_EXT_texture_rg GL_OES_texture_float GL_OES_texture_half_float",
      "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RED_EXT, GL_HALF_FLOAT_OES, GL_RED_EXT);
  ExpectValid(true, GL_RG_EXT, GL_HALF_FLOAT_OES, GL_RG_EXT);
  ExpectValid(true, GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT);
  ExpectValid(true, GL_RG_EXT, GL_UNSIGNED_BYTE, GL_RG_EXT);

  ExpectInvalidEnum(true, GL_RED_EXT, GL_BYTE, GL_RED_EXT);
  ExpectInvalidEnum(true, GL_RG_EXT, GL_BYTE, GL_RG_EXT);
  ExpectInvalidEnum(true, GL_RED_EXT, GL_SHORT, GL_RED_EXT);
  ExpectInvalidEnum(true, GL_RG_EXT, GL_SHORT, GL_RG_EXT);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithRGNoFloat) {
  SetupFeatureInfo("GL_ARB_texture_rg", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RED_EXT, GL_UNSIGNED_BYTE, GL_RED_EXT);
  ExpectValid(true, GL_RG_EXT, GL_UNSIGNED_BYTE, GL_RG_EXT);

  ExpectInvalidEnum(true, GL_RED_EXT, GL_HALF_FLOAT_OES, GL_RED_EXT);
  ExpectInvalidEnum(true, GL_RG_EXT, GL_HALF_FLOAT_OES, GL_RG_EXT);
}

TEST_F(TextureFormatTypeValidationTest, ES2OnTopOfES3) {
  SetupFeatureInfo("", "OpenGL ES 3.0", CONTEXT_TYPE_OPENGLES2);

  ExpectInvalidEnum(true, GL_RGB, GL_FLOAT, GL_RGB);
  ExpectInvalidEnum(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectInvalidEnum(true, GL_LUMINANCE, GL_FLOAT, GL_LUMINANCE);
  ExpectInvalidEnum(true, GL_LUMINANCE_ALPHA, GL_FLOAT, GL_LUMINANCE_ALPHA);
  ExpectInvalidEnum(true, GL_ALPHA, GL_FLOAT, GL_ALPHA);

  ExpectInvalidEnum(true, GL_SRGB_EXT, GL_UNSIGNED_BYTE, GL_SRGB_EXT);
  ExpectInvalidEnum(
      true, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE, GL_SRGB_ALPHA_EXT);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithOesTextureFloat) {
  SetupFeatureInfo("GL_OES_texture_float", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RGB, GL_FLOAT, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_FLOAT, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_FLOAT, GL_LUMINANCE_ALPHA);
  ExpectValid(true, GL_ALPHA, GL_FLOAT, GL_ALPHA);

  ExpectInvalidEnum(true, GL_RGB, GL_HALF_FLOAT_OES, GL_RGB);
  ExpectInvalidEnum(true, GL_RGBA, GL_HALF_FLOAT_OES, GL_RGBA);
  ExpectInvalidEnum(true, GL_LUMINANCE, GL_HALF_FLOAT_OES, GL_LUMINANCE);
  ExpectInvalidEnum(
      true, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_LUMINANCE_ALPHA);
  ExpectInvalidEnum(true, GL_ALPHA, GL_HALF_FLOAT_OES, GL_ALPHA);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithOesTextureFloatLinear) {
  SetupFeatureInfo("GL_OES_texture_float GL_OES_texture_float_linear",
                   "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RGB, GL_FLOAT, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_FLOAT, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_FLOAT, GL_LUMINANCE_ALPHA);
  ExpectValid(true, GL_ALPHA, GL_FLOAT, GL_ALPHA);

  ExpectInvalidEnum(true, GL_RGB, GL_HALF_FLOAT_OES, GL_RGB);
  ExpectInvalidEnum(true, GL_RGBA, GL_HALF_FLOAT_OES, GL_RGBA);
  ExpectInvalidEnum(true, GL_LUMINANCE, GL_HALF_FLOAT_OES, GL_LUMINANCE);
  ExpectInvalidEnum(
      true, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_LUMINANCE_ALPHA);
  ExpectInvalidEnum(true, GL_ALPHA, GL_HALF_FLOAT_OES, GL_ALPHA);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithOesTextureHalfFloat) {
  SetupFeatureInfo("GL_OES_texture_half_float", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RGB, GL_HALF_FLOAT_OES, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_HALF_FLOAT_OES, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_HALF_FLOAT_OES, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_LUMINANCE_ALPHA);
  ExpectValid(true, GL_ALPHA, GL_HALF_FLOAT_OES, GL_ALPHA);

  ExpectInvalidEnum(true, GL_RGB, GL_FLOAT, GL_RGB);
  ExpectInvalidEnum(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectInvalidEnum(true, GL_LUMINANCE, GL_FLOAT, GL_LUMINANCE);
  ExpectInvalidEnum(true, GL_LUMINANCE_ALPHA, GL_FLOAT, GL_LUMINANCE_ALPHA);
  ExpectInvalidEnum(true, GL_ALPHA, GL_FLOAT, GL_ALPHA);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithOesTextureHalfFloatLinear) {
  SetupFeatureInfo("GL_OES_texture_half_float GL_OES_texture_half_float_linear",
                   "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RGB, GL_HALF_FLOAT_OES, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_HALF_FLOAT_OES, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_HALF_FLOAT_OES, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_HALF_FLOAT_OES, GL_LUMINANCE_ALPHA);
  ExpectValid(true, GL_ALPHA, GL_HALF_FLOAT_OES, GL_ALPHA);

  ExpectInvalidEnum(true, GL_RGB, GL_FLOAT, GL_RGB);
  ExpectInvalidEnum(true, GL_RGBA, GL_FLOAT, GL_RGBA);
  ExpectInvalidEnum(true, GL_LUMINANCE, GL_FLOAT, GL_LUMINANCE);
  ExpectInvalidEnum(true, GL_LUMINANCE_ALPHA, GL_FLOAT, GL_LUMINANCE_ALPHA);
  ExpectInvalidEnum(true, GL_ALPHA, GL_FLOAT, GL_ALPHA);
}

TEST_F(TextureFormatTypeValidationTest, ES3Basic) {
  SetupFeatureInfo("", "OpenGL ES 3.0", CONTEXT_TYPE_OPENGLES3);

  ExpectValid(true, GL_ALPHA, GL_UNSIGNED_BYTE, GL_ALPHA);
  ExpectValid(true, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_RGB);
  ExpectValid(true, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, GL_RGBA);
  ExpectValid(true, GL_LUMINANCE, GL_UNSIGNED_BYTE, GL_LUMINANCE);
  ExpectValid(true, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA);

  ExpectValid(true, GL_RG, GL_BYTE, GL_RG8_SNORM);
  ExpectValid(true, GL_RG_INTEGER, GL_UNSIGNED_INT, GL_RG32UI);
  ExpectValid(true, GL_RG_INTEGER, GL_SHORT, GL_RG16I);
  ExpectValid(true, GL_RGB, GL_UNSIGNED_BYTE, GL_SRGB8);
  ExpectValid(true, GL_RGBA, GL_HALF_FLOAT, GL_RGBA16F);
  ExpectValid(true, GL_RGBA, GL_FLOAT, GL_RGBA16F);
  ExpectValid(true, GL_RGBA, GL_FLOAT, GL_RGBA32F);

  ExpectValid(
      true, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_COMPONENT16);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_DEPTH_COMPONENT24);
  ExpectValid(true, GL_DEPTH_COMPONENT, GL_FLOAT, GL_DEPTH_COMPONENT32F);
  ExpectValid(
      true, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH24_STENCIL8);
  ExpectValid(true, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
              GL_DEPTH32F_STENCIL8);

  ExpectInvalid(true, GL_RGB_INTEGER, GL_INT, GL_RGBA8);
}

TEST_F(TextureFormatTypeValidationTest, ES2WithTextureNorm16) {
  SetupFeatureInfo("GL_EXT_texture_norm16", "OpenGL ES 2.0",
                   CONTEXT_TYPE_OPENGLES2);

  ExpectValid(true, GL_RED, GL_UNSIGNED_SHORT, GL_RED);
}

TEST_F(TextureFormatTypeValidationTest, ES3WithTextureNorm16) {
  SetupFeatureInfo("GL_EXT_texture_norm16", "OpenGL ES 3.0",
                   CONTEXT_TYPE_OPENGLES3);

  ExpectValid(true, GL_RED, GL_UNSIGNED_SHORT, GL_R16_EXT);
}

}  // namespace gles2
}  // namespace gpu
