// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/test_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_version_info.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

namespace {

template<typename T>
T ConstructShaderVariable(
    GLenum type, GLint array_size, GLenum precision,
    bool static_use, const std::string& name) {
  T var;
  var.type = type;
  var.setArraySize(array_size);
  var.precision = precision;
  var.staticUse = static_use;
  var.name = name;
  var.mappedName = name;  // No name hashing.
  return var;
}

}  // namespace anonymous

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint TestHelper::kServiceBlackTexture2dId;
const GLuint TestHelper::kServiceDefaultTexture2dId;
const GLuint TestHelper::kServiceBlackTexture3dId;
const GLuint TestHelper::kServiceDefaultTexture3dId;
const GLuint TestHelper::kServiceBlackTexture2dArrayId;
const GLuint TestHelper::kServiceDefaultTexture2dArrayId;
const GLuint TestHelper::kServiceBlackTextureCubemapId;
const GLuint TestHelper::kServiceDefaultTextureCubemapId;
const GLuint TestHelper::kServiceBlackExternalTextureId;
const GLuint TestHelper::kServiceDefaultExternalTextureId;
const GLuint TestHelper::kServiceBlackRectangleTextureId;
const GLuint TestHelper::kServiceDefaultRectangleTextureId;

const GLint TestHelper::kMaxSamples;
const GLint TestHelper::kMaxRenderbufferSize;
const GLint TestHelper::kMaxTextureSize;
const GLint TestHelper::kMaxCubeMapTextureSize;
const GLint TestHelper::kMaxRectangleTextureSize;
const GLint TestHelper::kMax3DTextureSize;
const GLint TestHelper::kMaxArrayTextureLayers;
const GLint TestHelper::kNumVertexAttribs;
const GLint TestHelper::kNumTextureUnits;
const GLint TestHelper::kMaxTextureImageUnits;
const GLint TestHelper::kMaxVertexTextureImageUnits;
const GLint TestHelper::kMaxFragmentUniformVectors;
const GLint TestHelper::kMaxFragmentUniformComponents;
const GLint TestHelper::kMaxVaryingVectors;
const GLint TestHelper::kMaxVaryingFloats;
const GLint TestHelper::kMaxVertexUniformVectors;
const GLint TestHelper::kMaxVertexUniformComponents;
const GLint TestHelper::kMaxVertexOutputComponents;
const GLint TestHelper::kMaxFragmentInputComponents;
const GLint TestHelper::kMaxProgramTexelOffset;
const GLint TestHelper::kMinProgramTexelOffset;
const GLint TestHelper::kMaxTransformFeedbackSeparateAttribs;
const GLint TestHelper::kMaxUniformBufferBindings;
const GLint TestHelper::kUniformBufferOffsetAlignment;
#endif

std::vector<std::string> TestHelper::split_extensions_;

void TestHelper::SetupTextureInitializationExpectations(
    ::gl::MockGLInterface* gl,
    GLenum target,
    bool use_default_textures) {
  InSequence sequence;

  bool needs_initialization = (target != GL_TEXTURE_EXTERNAL_OES);
  bool needs_faces = (target == GL_TEXTURE_CUBE_MAP);
  bool is_3d_or_2d_array_target = (target == GL_TEXTURE_3D ||
      target == GL_TEXTURE_2D_ARRAY);

  static GLuint texture_2d_ids[] = {
    kServiceBlackTexture2dId,
    kServiceDefaultTexture2dId };
  static GLuint texture_3d_ids[] = {
    kServiceBlackTexture3dId,
    kServiceDefaultTexture3dId };
  static GLuint texture_2d_array_ids[] = {
    kServiceBlackTexture2dArrayId,
    kServiceDefaultTexture2dArrayId };
  static GLuint texture_cube_map_ids[] = {
    kServiceBlackTextureCubemapId,
    kServiceDefaultTextureCubemapId };
  static GLuint texture_external_oes_ids[] = {
    kServiceBlackExternalTextureId,
    kServiceDefaultExternalTextureId };
  static GLuint texture_rectangle_arb_ids[] = {
    kServiceBlackRectangleTextureId,
    kServiceDefaultRectangleTextureId };

  const GLuint* texture_ids = nullptr;
  switch (target) {
    case GL_TEXTURE_2D:
      texture_ids = &texture_2d_ids[0];
      break;
    case GL_TEXTURE_3D:
      texture_ids = &texture_3d_ids[0];
      break;
    case GL_TEXTURE_2D_ARRAY:
      texture_ids = &texture_2d_array_ids[0];
      break;
    case GL_TEXTURE_CUBE_MAP:
      texture_ids = &texture_cube_map_ids[0];
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      texture_ids = &texture_external_oes_ids[0];
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      texture_ids = &texture_rectangle_arb_ids[0];
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  int array_size = use_default_textures ? 2 : 1;

  EXPECT_CALL(*gl, GenTextures(array_size, _))
      .WillOnce(SetArrayArgument<1>(texture_ids,
                                    texture_ids + array_size))
          .RetiresOnSaturation();
  for (int ii = 0; ii < array_size; ++ii) {
    EXPECT_CALL(*gl, BindTexture(target, texture_ids[ii]))
        .Times(1)
        .RetiresOnSaturation();
    if (needs_initialization) {
      if (needs_faces) {
        static GLenum faces[] = {
          GL_TEXTURE_CUBE_MAP_POSITIVE_X,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
        };
        for (size_t face = 0; face < std::size(faces); ++face) {
          EXPECT_CALL(*gl, TexImage2D(faces[face], 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                                      GL_UNSIGNED_BYTE, _))
              .Times(1)
              .RetiresOnSaturation();
        }
      } else {
        if (is_3d_or_2d_array_target) {
          EXPECT_CALL(*gl, TexImage3D(target, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
                                      GL_UNSIGNED_BYTE, _))
              .Times(1)
              .RetiresOnSaturation();
        } else {
          EXPECT_CALL(*gl, TexImage2D(target, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                                      GL_UNSIGNED_BYTE, _))
              .Times(1)
              .RetiresOnSaturation();
        }
      }
    }
  }
  EXPECT_CALL(*gl, BindTexture(target, 0))
      .Times(1)
      .RetiresOnSaturation();
}

void TestHelper::SetupTextureManagerInitExpectations(
    ::gl::MockGLInterface* gl,
    bool is_es3_enabled,
    bool is_es3_capable,
    const gfx::ExtensionSet& extensions,
    bool use_default_textures) {
  InSequence sequence;

  if (is_es3_capable) {
    EXPECT_CALL(*gl, BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0))
        .Times(1)
        .RetiresOnSaturation();
  }

  SetupTextureInitializationExpectations(
      gl, GL_TEXTURE_2D, use_default_textures);
  SetupTextureInitializationExpectations(
      gl, GL_TEXTURE_CUBE_MAP, use_default_textures);

  if (is_es3_enabled) {
    SetupTextureInitializationExpectations(
        gl, GL_TEXTURE_3D, use_default_textures);
    SetupTextureInitializationExpectations(
        gl, GL_TEXTURE_2D_ARRAY, use_default_textures);
  }

  bool ext_image_external =
      gfx::HasExtension(extensions, "GL_OES_EGL_image_external");
  bool angle_texture_rectangle =
      gfx::HasExtension(extensions, "GL_ANGLE_texture_rectangle");

  if (ext_image_external) {
    SetupTextureInitializationExpectations(
        gl, GL_TEXTURE_EXTERNAL_OES, use_default_textures);
  }
  if (angle_texture_rectangle) {
    SetupTextureInitializationExpectations(
        gl, GL_TEXTURE_RECTANGLE_ARB, use_default_textures);
  }
}

void TestHelper::SetupTextureDestructionExpectations(
    ::gl::MockGLInterface* gl,
    GLenum target,
    bool use_default_textures) {
  if (!use_default_textures)
    return;

  GLuint texture_id = 0;
  switch (target) {
    case GL_TEXTURE_2D:
      texture_id = kServiceDefaultTexture2dId;
      break;
    case GL_TEXTURE_3D:
      texture_id = kServiceDefaultTexture3dId;
      break;
    case GL_TEXTURE_2D_ARRAY:
      texture_id = kServiceDefaultTexture2dArrayId;
      break;
    case GL_TEXTURE_CUBE_MAP:
      texture_id = kServiceDefaultTextureCubemapId;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      texture_id = kServiceDefaultExternalTextureId;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      texture_id = kServiceDefaultRectangleTextureId;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  EXPECT_CALL(*gl, DeleteTextures(1, Pointee(texture_id)))
      .Times(1)
      .RetiresOnSaturation();
}

void TestHelper::SetupTextureManagerDestructionExpectations(
    ::gl::MockGLInterface* gl,
    bool is_es3_enabled,
    const gfx::ExtensionSet& extensions,
    bool use_default_textures) {
  SetupTextureDestructionExpectations(gl, GL_TEXTURE_2D, use_default_textures);
  SetupTextureDestructionExpectations(
      gl, GL_TEXTURE_CUBE_MAP, use_default_textures);

  if (is_es3_enabled) {
    SetupTextureDestructionExpectations(
        gl, GL_TEXTURE_3D, use_default_textures);
    SetupTextureDestructionExpectations(
        gl, GL_TEXTURE_2D_ARRAY,use_default_textures);
  }

  bool ext_image_external =
      gfx::HasExtension(extensions, "GL_OES_EGL_image_external");
  bool arb_texture_rectangle =
      gfx::HasExtension(extensions, "GL_ARB_texture_rectangle");

  if (ext_image_external) {
    SetupTextureDestructionExpectations(
        gl, GL_TEXTURE_EXTERNAL_OES, use_default_textures);
  }
  if (arb_texture_rectangle) {
    SetupTextureDestructionExpectations(
        gl, GL_TEXTURE_RECTANGLE_ARB, use_default_textures);
  }

  EXPECT_CALL(*gl, DeleteTextures(TextureManager::kNumDefaultTextures, _))
      .Times(1)
      .RetiresOnSaturation();
}

void TestHelper::SetupContextGroupInitExpectations(
    ::gl::MockGLInterface* gl,
    const DisallowedFeatures& disallowed_features,
    const char* extensions,
    const char* gl_version,
    ContextType context_type,
    bool bind_generates_resource) {
  InSequence sequence;

  bool enable_es3 = !(context_type == CONTEXT_TYPE_OPENGLES2 ||
                      context_type == CONTEXT_TYPE_WEBGL1);

  gfx::ExtensionSet extension_set(gfx::MakeExtensionSet(extensions));
  gl::GLVersionInfo gl_info(gl_version, "", extension_set);

  SetupFeatureInfoInitExpectationsWithGLVersion(gl, extensions, "", gl_version,
      context_type);
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, _))
      .WillOnce(SetArgPointee<1>(kMaxRenderbufferSize))
      .RetiresOnSaturation();
  if (gfx::HasExtension(extension_set, "GL_EXT_framebuffer_multisample") ||
      gfx::HasExtension(extension_set,
                        "GL_EXT_multisampled_render_to_texture") ||
      gl_info.is_es3) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_SAMPLES, _))
        .WillOnce(SetArgPointee<1>(kMaxSamples))
        .RetiresOnSaturation();
  } else if (gfx::HasExtension(extension_set,
                               "GL_IMG_multisampled_render_to_texture")) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_SAMPLES_IMG, _))
        .WillOnce(SetArgPointee<1>(kMaxSamples))
        .RetiresOnSaturation();
  }

  if (enable_es3 ||
      (!enable_es3 &&
       (gfx::HasExtension(extension_set, "GL_EXT_draw_buffers") ||
        (gl_info.is_es3 &&
         gfx::HasExtension(extension_set, "GL_NV_draw_buffers"))))) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  if (gfx::HasExtension(extension_set, "GL_EXT_blend_func_extended")) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  if (gl_info.IsAtLeastGLES(3, 0)) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, _))
        .WillOnce(SetArgPointee<1>(kMaxTransformFeedbackSeparateAttribs))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, _))
        .WillOnce(SetArgPointee<1>(kMaxUniformBufferBindings))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, _))
        .WillOnce(SetArgPointee<1>(kUniformBufferOffsetAlignment))
        .RetiresOnSaturation();
  }

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
      .WillOnce(SetArgPointee<1>(kNumVertexAttribs))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgPointee<1>(kNumTextureUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_TEXTURE_SIZE, _))
      .WillOnce(SetArgPointee<1>(kMaxTextureSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, _))
      .WillOnce(SetArgPointee<1>(kMaxCubeMapTextureSize))
      .RetiresOnSaturation();
  if (gl_info.IsAtLeastGLES(3, 0)) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_3D_TEXTURE_SIZE, _))
        .WillOnce(SetArgPointee<1>(kMax3DTextureSize))
        .RetiresOnSaturation();
  }
  if (gl_info.IsAtLeastGLES(3, 0)) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, _))
        .WillOnce(SetArgPointee<1>(kMaxArrayTextureLayers))
        .RetiresOnSaturation();
  }
  if (gfx::HasExtension(extension_set, "GL_ANGLE_texture_rectangle")) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE, _))
        .WillOnce(SetArgPointee<1>(kMaxRectangleTextureSize))
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgPointee<1>(kMaxTextureImageUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgPointee<1>(kMaxVertexTextureImageUnits))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, _))
      .WillOnce(SetArgPointee<1>(kMaxFragmentUniformVectors))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VARYING_VECTORS, _))
      .WillOnce(SetArgPointee<1>(kMaxVaryingVectors))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, _))
      .WillOnce(SetArgPointee<1>(kMaxVertexUniformVectors))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, _))
      .Times(testing::Between(0, 1))
      .WillRepeatedly(SetArgPointee<1>(kMaxVertexOutputComponents))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS, _))
      .Times(testing::Between(0, 1))
      .WillRepeatedly(SetArgPointee<1>(kMaxFragmentInputComponents))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, _))
      .Times(testing::Between(0, 1))
      .WillRepeatedly(SetArgPointee<1>(kMaxProgramTexelOffset))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, _))
      .Times(testing::Between(0, 1))
      .WillRepeatedly(SetArgPointee<1>(kMinProgramTexelOffset))
      .RetiresOnSaturation();

  bool use_default_textures = bind_generates_resource;
  SetupTextureManagerInitExpectations(gl, enable_es3,
                                      gl_info.IsAtLeastGLES(3, 0),
                                      extension_set, use_default_textures);
}

void TestHelper::SetupFeatureInfoInitExpectations(::gl::MockGLInterface* gl,
                                                  const char* extensions) {
  SetupFeatureInfoInitExpectationsWithGLVersion(
      gl, extensions, "ANGLE", "OpenGL ES 2.0", CONTEXT_TYPE_OPENGLES2);
}

void TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
    ::gl::MockGLInterface* gl,
    const char* extensions,
    const char* gl_renderer,
    const char* gl_version,
    ContextType context_type) {
  InSequence sequence;

  bool enable_es3 = context_type == CONTEXT_TYPE_WEBGL2 ||
      context_type == CONTEXT_TYPE_OPENGLES3;

  gfx::ExtensionSet extension_set(gfx::MakeExtensionSet(extensions));
  // Persistent storage is needed for the split extension string.
  split_extensions_ =
      std::vector<std::string>(extension_set.begin(), extension_set.end());
  gl::GLVersionInfo gl_info(gl_version, gl_renderer, extension_set);
  EXPECT_CALL(*gl, GetString(GL_EXTENSIONS))
      .WillOnce(Return(reinterpret_cast<const uint8_t*>(extensions)))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl, GetString(GL_VERSION))
      .WillOnce(Return(reinterpret_cast<const uint8_t*>(gl_version)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetString(GL_RENDERER))
      .WillOnce(Return(reinterpret_cast<const uint8_t*>(gl_renderer)))
      .RetiresOnSaturation();

  if (gl_info.is_es3 ||
      gfx::HasExtension(extension_set, "GL_ARB_pixel_buffer_object") ||
      gfx::HasExtension(extension_set, "GL_NV_pixel_buffer_object")) {
    EXPECT_CALL(*gl, GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, _))
      .WillOnce(SetArgPointee<1>(0))
      .RetiresOnSaturation();
  }

  if (gfx::HasExtension(extension_set, "GL_ARB_texture_float") ||
      (gl_info.is_es3 &&
       gfx::HasExtension(extension_set, "GL_OES_texture_float") &&
       gfx::HasExtension(extension_set, "GL_EXT_color_buffer_float"))) {
    static const GLuint tx_ids[] = {101, 102};
    static const GLuint fb_ids[] = {103, 104};
    const GLsizei width = 16;
    EXPECT_CALL(*gl, GetIntegerv(GL_FRAMEBUFFER_BINDING, _))
        .WillOnce(SetArgPointee<1>(fb_ids[0]))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_TEXTURE_BINDING_2D, _))
        .WillOnce(SetArgPointee<1>(tx_ids[0]))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GenTextures(1, _))
        .WillOnce(SetArrayArgument<1>(tx_ids + 1, tx_ids + 2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GenFramebuffersEXT(1, _))
        .WillOnce(SetArrayArgument<1>(fb_ids + 1, fb_ids + 2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, tx_ids[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        GL_NEAREST))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, width, 0,
        GL_RGBA, GL_FLOAT, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindFramebufferEXT(GL_FRAMEBUFFER, fb_ids[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, FramebufferTexture2DEXT(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tx_ids[1], 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
    GLenum status_rgba = GL_FRAMEBUFFER_COMPLETE;
    EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, width, 0,
        GL_RGB, GL_FLOAT, _))
        .Times(1)
        .RetiresOnSaturation();
    if (gl_info.is_es3) {
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .WillOnce(Return(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT))
          .RetiresOnSaturation();
    } else {
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
          .RetiresOnSaturation();
    }

    if (status_rgba == GL_FRAMEBUFFER_COMPLETE && enable_es3) {
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, width,
          0, GL_RED, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, width,
          0, GL_RG, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, width,
          0, GL_RGBA, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, width,
          0, GL_RED, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, width,
          0, GL_RG, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F,
          width, width, 0, GL_RGB, GL_FLOAT, _))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
    }
    if (!enable_es3 &&
        !gfx::HasExtension(extension_set, "GL_EXT_color_buffer_half_float") &&
        gl_info.IsAtLeastGLES(3, 0)) {
      EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, width, 0,
                                  GL_RGBA, GL_HALF_FLOAT, nullptr))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
          .Times(1)
          .RetiresOnSaturation();
    }

    EXPECT_CALL(*gl, DeleteFramebuffersEXT(1, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, DeleteTextures(1, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindFramebufferEXT(GL_FRAMEBUFFER, fb_ids[0]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, tx_ids[0]))
        .Times(1)
        .RetiresOnSaturation();
#if DCHECK_IS_ON()
    EXPECT_CALL(*gl, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
#endif
  }

  if (enable_es3 ||
      (!enable_es3 &&
       (gfx::HasExtension(extension_set, "GL_EXT_draw_buffers") ||
        gfx::HasExtension(extension_set, "GL_ARB_draw_buffers") ||
        (gl_info.is_es3 &&
         gfx::HasExtension(extension_set, "GL_NV_draw_buffers"))))) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  // These expectations are for IsGL_REDSupportedOnFBOs(), which is
  // skipped universally on macOS, and by default (with a Finch
  // kill-switch) on Android.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
  if (gl_info.is_es3 || gfx::HasExtension(extension_set, "GL_EXT_texture_rg") ||
      (gfx::HasExtension(extension_set, "GL_ARB_texture_rg"))) {
#if DCHECK_IS_ON()
    EXPECT_CALL(*gl, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
#endif
    static const GLuint tx_ids[] = {101, 102};
    static const GLuint fb_ids[] = {103, 104};
    const GLsizei width = 8;
    EXPECT_CALL(*gl, GetIntegerv(GL_FRAMEBUFFER_BINDING, _))
        .WillOnce(SetArgPointee<1>(fb_ids[0]))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetIntegerv(GL_TEXTURE_BINDING_2D, _))
        .WillOnce(SetArgPointee<1>(tx_ids[0]))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GenTextures(1, _))
        .WillOnce(SetArrayArgument<1>(tx_ids + 1, tx_ids + 2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, tx_ids[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, _, width, width, 0,
        GL_RED_EXT, GL_UNSIGNED_BYTE, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GenFramebuffersEXT(1, _))
        .WillOnce(SetArrayArgument<1>(fb_ids + 1, fb_ids + 2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindFramebufferEXT(GL_FRAMEBUFFER, fb_ids[1]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, FramebufferTexture2DEXT(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tx_ids[1], 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, DeleteFramebuffersEXT(1, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, DeleteTextures(1, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindFramebufferEXT(GL_FRAMEBUFFER, fb_ids[0]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, tx_ids[0]))
        .Times(1)
        .RetiresOnSaturation();
#if DCHECK_IS_ON()
    EXPECT_CALL(*gl, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
#endif
  }
#endif  // !BUILDFLAG(IS_MAC)
}

void TestHelper::SetupProgramSuccessExpectations(
    ::gl::MockGLInterface* gl,
    const FeatureInfo* feature_info,
    AttribInfo* attribs,
    size_t num_attribs,
    UniformInfo* uniforms,
    size_t num_uniforms,
    VaryingInfo* varyings,
    size_t num_varyings,
    ProgramOutputInfo* program_outputs,
    size_t num_program_outputs,
    GLuint service_id) {
  EXPECT_CALL(*gl, GetProgramiv(service_id, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetProgramiv(service_id, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgPointee<2>(num_attribs))
      .RetiresOnSaturation();
  size_t max_attrib_len = 0;
  for (size_t ii = 0; ii < num_attribs; ++ii) {
    size_t len = strlen(attribs[ii].name) + 1;
    max_attrib_len = std::max(max_attrib_len, len);
  }
  EXPECT_CALL(*gl, GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgPointee<2>(max_attrib_len))
      .RetiresOnSaturation();

  for (size_t ii = 0; ii < num_attribs; ++ii) {
    const AttribInfo& info = attribs[ii];
    EXPECT_CALL(*gl,
                GetActiveAttrib(service_id, ii, max_attrib_len, _, _, _, _))
        .WillOnce(DoAll(
            SetArgPointee<3>(strlen(info.name)), SetArgPointee<4>(info.size),
            SetArgPointee<5>(info.type),
            SetArrayArgument<6>(info.name, info.name + strlen(info.name) + 1)))
        .RetiresOnSaturation();
    if (!ProgramManager::HasBuiltInPrefix(info.name)) {
      EXPECT_CALL(*gl, GetAttribLocation(service_id, StrEq(info.name)))
          .WillOnce(Return(info.location))
          .RetiresOnSaturation();
    }
  }
  EXPECT_CALL(*gl, GetProgramiv(service_id, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(num_uniforms))
      .RetiresOnSaturation();

  if (num_uniforms > 0) {
    size_t max_uniform_len = 0;
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      size_t len = strlen(uniforms[ii].name) + 1;
      max_uniform_len = std::max(max_uniform_len, len);
    }
    EXPECT_CALL(*gl, GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
        .WillOnce(SetArgPointee<2>(max_uniform_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      EXPECT_CALL(*gl,
                  GetActiveUniform(service_id, ii, max_uniform_len, _, _, _, _))
          .WillOnce(DoAll(SetArgPointee<3>(strlen(info.name)),
                          SetArgPointee<4>(info.size),
                          SetArgPointee<5>(info.type),
                          SetArrayArgument<6>(
                              info.name, info.name + strlen(info.name) + 1)))
          .RetiresOnSaturation();

      // Corresponds to early out in Program::UpdateUniforms
      if (!info.size)
        return;

      if (info.real_location != -1) {
        EXPECT_CALL(*gl, GetUniformLocation(service_id, StrEq(info.name)))
            .WillOnce(Return(info.real_location))
            .RetiresOnSaturation();
      }
      if (info.size > 1) {
        std::string base_name = info.name;
        size_t array_pos = base_name.rfind("[0]");
        if (base_name.size() > 3 && array_pos == base_name.size() - 3) {
          base_name = base_name.substr(0, base_name.size() - 3);
        }
        for (GLsizei jj = 1; jj < info.size; ++jj) {
          std::string element_name(std::string(base_name) + "[" +
                                   base::NumberToString(jj) + "]");
          EXPECT_CALL(*gl, GetUniformLocation(service_id, StrEq(element_name)))
              .WillOnce(Return(info.real_location + jj * 2))
              .RetiresOnSaturation();
        }
      }
    }
  }

  if (feature_info->gl_version_info().IsAtLeastGLES(3, 0) &&
      !feature_info->disable_shader_translator()) {
    for (size_t ii = 0; ii < num_program_outputs; ++ii) {
      ProgramOutputInfo& info = program_outputs[ii];
      if (ProgramManager::HasBuiltInPrefix(info.name))
        continue;

      EXPECT_CALL(*gl, GetFragDataLocation(service_id, StrEq(info.name)))
          .WillOnce(Return(info.color_name))
          .RetiresOnSaturation();
      if (feature_info->feature_flags().ext_blend_func_extended) {
        EXPECT_CALL(*gl, GetFragDataIndex(service_id, StrEq(info.name)))
            .WillOnce(Return(info.index))
            .RetiresOnSaturation();
      } else {
        // Test case must not use indices, or the context of the testcase has to
        // support the dual source blending.
        DCHECK(info.index == 0);
      }
    }
  }
}

void TestHelper::SetupShaderExpectations(::gl::MockGLInterface* gl,
                                         const FeatureInfo* feature_info,
                                         AttribInfo* attribs,
                                         size_t num_attribs,
                                         UniformInfo* uniforms,
                                         size_t num_uniforms,
                                         GLuint service_id) {
  InSequence s;

  EXPECT_CALL(*gl, LinkProgram(service_id)).Times(1).RetiresOnSaturation();

  SetupProgramSuccessExpectations(gl, feature_info, attribs, num_attribs,
                                  uniforms, num_uniforms, nullptr, 0, nullptr,
                                  0, service_id);
}

void TestHelper::SetupShaderExpectationsWithVaryings(
    ::gl::MockGLInterface* gl,
    const FeatureInfo* feature_info,
    AttribInfo* attribs,
    size_t num_attribs,
    UniformInfo* uniforms,
    size_t num_uniforms,
    VaryingInfo* varyings,
    size_t num_varyings,
    ProgramOutputInfo* program_outputs,
    size_t num_program_outputs,
    GLuint service_id) {
  InSequence s;

  EXPECT_CALL(*gl,
      LinkProgram(service_id))
      .Times(1)
      .RetiresOnSaturation();

  SetupProgramSuccessExpectations(
      gl, feature_info, attribs, num_attribs, uniforms, num_uniforms, varyings,
      num_varyings, program_outputs, num_program_outputs, service_id);
}

void TestHelper::DoBufferData(::gl::MockGLInterface* gl,
                              MockErrorState* error_state,
                              BufferManager* manager,
                              Buffer* buffer,
                              GLenum target,
                              GLsizeiptr size,
                              GLenum usage,
                              const GLvoid* data,
                              GLenum error) {
  EXPECT_CALL(*error_state, CopyRealGLErrorsToWrapper(_, _, _))
      .Times(1)
      .RetiresOnSaturation();
  if (manager->IsUsageClientSideArray(usage)) {
    EXPECT_CALL(*gl, BufferData(target, 0, _, usage))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl, BufferData(target, size, _, usage))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*error_state, PeekGLError(_, _, _))
      .WillOnce(Return(error))
      .RetiresOnSaturation();
  manager->DoBufferData(error_state, buffer, target, size, usage, data);
}

void TestHelper::SetTexParameteriWithExpectations(::gl::MockGLInterface* gl,
                                                  MockErrorState* error_state,
                                                  TextureManager* manager,
                                                  TextureRef* texture_ref,
                                                  GLenum pname,
                                                  GLint value,
                                                  GLenum error) {
  if (error == GL_NO_ERROR) {
    EXPECT_CALL(*gl, TexParameteri(texture_ref->texture()->target(),
                                   pname, value))
        .Times(1)
        .RetiresOnSaturation();
  } else if (error == GL_INVALID_ENUM) {
    EXPECT_CALL(*error_state, SetGLErrorInvalidEnum(_, _, _, value, _))
        .Times(1)
        .RetiresOnSaturation();
  } else {
    EXPECT_CALL(*error_state, SetGLErrorInvalidParami(_, _, error, _, _, _))
        .Times(1)
        .RetiresOnSaturation();
  }
  manager->SetParameteri("", error_state, texture_ref, pname, value);
}

// static
void TestHelper::SetShaderStates(
    ::gl::MockGLInterface* gl,
    Shader* shader,
    bool expected_valid,
    const std::string* const expected_log_info,
    const std::string* const expected_translated_source,
    const int* const expected_shader_version,
    const AttributeMap* const expected_attrib_map,
    const UniformMap* const expected_uniform_map,
    const VaryingMap* const expected_varying_map,
    const InterfaceBlockMap* const expected_interface_block_map,
    const OutputVariableList* const expected_output_variable_list,
    OptionsAffectingCompilationString* options_affecting_compilation) {
  const std::string empty_log_info;
  const std::string* log_info = (expected_log_info && !expected_valid) ?
      expected_log_info : &empty_log_info;
  const std::string empty_translated_source;
  const std::string* translated_source =
      (expected_translated_source && expected_valid) ?
          expected_translated_source : &empty_translated_source;
  int default_shader_version = 100;
  const int* shader_version = (expected_shader_version && expected_valid) ?
      expected_shader_version : &default_shader_version;
  const AttributeMap empty_attrib_map;
  const AttributeMap* attrib_map = (expected_attrib_map && expected_valid) ?
      expected_attrib_map : &empty_attrib_map;
  const UniformMap empty_uniform_map;
  const UniformMap* uniform_map = (expected_uniform_map && expected_valid) ?
      expected_uniform_map : &empty_uniform_map;
  const VaryingMap empty_varying_map;
  const VaryingMap* varying_map = (expected_varying_map && expected_valid) ?
      expected_varying_map : &empty_varying_map;
  const InterfaceBlockMap empty_interface_block_map;
  const InterfaceBlockMap* interface_block_map =
      (expected_interface_block_map && expected_valid) ?
      expected_interface_block_map : &empty_interface_block_map;
  const OutputVariableList empty_output_variable_list;
  const OutputVariableList* output_variable_list =
      (expected_output_variable_list && expected_valid)
          ? expected_output_variable_list
          : &empty_output_variable_list;

  MockShaderTranslator* mock_translator = new MockShaderTranslator;
  scoped_refptr<ShaderTranslatorInterface> translator(mock_translator);
  EXPECT_CALL(*mock_translator, Translate(_,
                                          NotNull(),   // log_info
                                          NotNull(),   // translated_source
                                          NotNull(),   // shader_version
                                          NotNull(),   // attrib_map
                                          NotNull(),   // uniform_map
                                          NotNull(),   // varying_map
                                          NotNull(),   // interface_block_map
                                          NotNull()))  // output_variable_list
      .WillOnce(DoAll(
          SetArgPointee<1>(*log_info), SetArgPointee<2>(*translated_source),
          SetArgPointee<3>(*shader_version), SetArgPointee<4>(*attrib_map),
          SetArgPointee<5>(*uniform_map), SetArgPointee<6>(*varying_map),
          SetArgPointee<7>(*interface_block_map),
          SetArgPointee<8>(*output_variable_list), Return(expected_valid)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_translator, GetStringForOptionsThatWouldAffectCompilation())
      .WillOnce(Return(options_affecting_compilation))
      .RetiresOnSaturation();
  if (expected_valid) {
    EXPECT_CALL(*gl, ShaderSource(shader->service_id(), 1, _, nullptr))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, CompileShader(shader->service_id()))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetShaderiv(shader->service_id(), GL_COMPILE_STATUS,
                                 NotNull()))  // status
        .WillOnce(SetArgPointee<2>(GL_TRUE))
        .RetiresOnSaturation();
  }
  shader->RequestCompile(translator, Shader::kGL);
  shader->DoCompile();
}

// static
void TestHelper::SetShaderStates(::gl::MockGLInterface* gl,
                                 Shader* shader,
                                 bool valid) {
  SetShaderStates(gl, shader, valid, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr);
}

// static
void TestHelper::SetShaderStates(
    ::gl::MockGLInterface* gl,
    Shader* shader,
    bool valid,
    const std::string& options_affecting_compilation) {
  scoped_refptr<OptionsAffectingCompilationString> options =
      base::MakeRefCounted<OptionsAffectingCompilationString>(
          options_affecting_compilation);
  SetShaderStates(gl, shader, valid, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, options.get());
}

// static
sh::Attribute TestHelper::ConstructAttribute(
    GLenum type, GLint array_size, GLenum precision,
    bool static_use, const std::string& name) {
  return ConstructShaderVariable<sh::Attribute>(
      type, array_size, precision, static_use, name);
}

// static
sh::Uniform TestHelper::ConstructUniform(
    GLenum type, GLint array_size, GLenum precision,
    bool static_use, const std::string& name) {
  return ConstructShaderVariable<sh::Uniform>(
      type, array_size, precision, static_use, name);
}

// static
sh::Varying TestHelper::ConstructVarying(
    GLenum type, GLint array_size, GLenum precision,
    bool static_use, const std::string& name) {
  return ConstructShaderVariable<sh::Varying>(
      type, array_size, precision, static_use, name);
}

// static
sh::InterfaceBlockField TestHelper::ConstructInterfaceBlockField(
    GLenum type,
    GLint array_size,
    GLenum precision,
    bool static_use,
    const std::string& name) {
  return ConstructShaderVariable<sh::InterfaceBlockField>(
      type, array_size, precision, static_use, name);
}

// static
sh::InterfaceBlock TestHelper::ConstructInterfaceBlock(
    GLint array_size,
    sh::BlockLayoutType layout,
    bool is_row_major_layout,
    bool static_use,
    const std::string& name,
    const std::string& instance_name,
    const std::vector<sh::InterfaceBlockField>& fields) {
  sh::InterfaceBlock var;
  var.arraySize = array_size;
  var.layout = layout;
  var.isRowMajorLayout = is_row_major_layout;
  var.staticUse = static_use;
  var.name = name;
  var.mappedName = name;  // No name hashing.
  var.instanceName = instance_name;
  var.fields = fields;
  return var;
}

sh::OutputVariable TestHelper::ConstructOutputVariable(
    GLenum type,
    GLint array_size,
    GLenum precision,
    bool static_use,
    const std::string& name) {
  return ConstructShaderVariable<sh::OutputVariable>(
      type, array_size, precision, static_use, name);
}

}  // namespace gles2
}  // namespace gpu
