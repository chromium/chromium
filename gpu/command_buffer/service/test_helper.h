// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/shader_translator.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"

namespace gpu {
namespace gles2 {

struct DisallowedFeatures;
class Buffer;
class BufferManager;
class FeatureInfo;
class MockErrorState;
class Shader;
class TextureRef;
class TextureManager;

class TestHelper {
 public:
  static const GLuint kServiceBlackTexture2dId = 701;
  static const GLuint kServiceDefaultTexture2dId = 702;
  static const GLuint kServiceBlackTexture3dId = 703;
  static const GLuint kServiceDefaultTexture3dId = 704;
  static const GLuint kServiceBlackTexture2dArrayId = 705;
  static const GLuint kServiceDefaultTexture2dArrayId = 706;
  static const GLuint kServiceBlackTextureCubemapId = 707;
  static const GLuint kServiceDefaultTextureCubemapId = 708;
  static const GLuint kServiceBlackExternalTextureId = 709;
  static const GLuint kServiceDefaultExternalTextureId = 710;
  static const GLuint kServiceBlackRectangleTextureId = 711;
  static const GLuint kServiceDefaultRectangleTextureId = 712;

  static const GLint kMaxSamples = 4;
  static const GLint kMaxRenderbufferSize = 1024;
  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 2048;
  static const GLint kMax3DTextureSize = 1024;
  static const GLint kMaxArrayTextureLayers = 256;
  static const GLint kMaxRectangleTextureSize = 64;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxFragmentUniformComponents =
      kMaxFragmentUniformVectors * 4;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVaryingFloats = kMaxVaryingVectors * 4;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kMaxVertexUniformComponents = kMaxVertexUniformVectors * 4;
  static const GLint kMaxVertexOutputComponents = 64;
  static const GLint kMaxFragmentInputComponents = 60;
  static const GLint kMaxProgramTexelOffset = 7;
  static const GLint kMinProgramTexelOffset = -8;

  static const GLint kMaxTransformFeedbackSeparateAttribs = 4;
  static const GLint kMaxUniformBufferBindings = 24;
  static const GLint kUniformBufferOffsetAlignment = 1;

  struct AttribInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  struct UniformInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint fake_location;
    GLint real_location;
    GLint desired_location;
    const char* good_name;
  };

  struct VaryingInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint fake_location;
    GLint real_location;
    GLint desired_location;
  };
  struct ProgramOutputInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint color_name;
    GLuint index;
  };

  static void SetupContextGroupInitExpectations(
      ::gl::MockGLInterface* gl,
      const DisallowedFeatures& disallowed_features,
      const char* extensions,
      const char* gl_version,
      ContextType context_type,
      bool bind_generates_resource);
  static void SetupFeatureInfoInitExpectations(::gl::MockGLInterface* gl,
                                               const char* extensions);
  static void SetupFeatureInfoInitExpectationsWithGLVersion(
      ::gl::MockGLInterface* gl,
      const char* extensions,
      const char* gl_renderer,
      const char* gl_version,
      ContextType context_type);
  static void SetupTextureManagerInitExpectations(
      ::gl::MockGLInterface* gl,
      bool is_es3_enabled,
      bool is_es3_capable,
      const gfx::ExtensionSet& extensions,
      bool use_default_textures);
  static void SetupTextureManagerDestructionExpectations(
      ::gl::MockGLInterface* gl,
      bool is_es3_enabled,
      const gfx::ExtensionSet& extensions,
      bool use_default_textures);

  static void SetupShaderExpectations(::gl::MockGLInterface* gl,
                                      const FeatureInfo* feature_info,
                                      AttribInfo* attribs,
                                      size_t num_attribs,
                                      UniformInfo* uniforms,
                                      size_t num_uniforms,
                                      GLuint service_id);

  static void SetupShaderExpectationsWithVaryings(
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
      GLuint service_id);

  static void SetupProgramSuccessExpectations(
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
      GLuint service_id);

  static void DoBufferData(::gl::MockGLInterface* gl,
                           MockErrorState* error_state,
                           BufferManager* manager,
                           Buffer* buffer,
                           GLenum target,
                           GLsizeiptr size,
                           GLenum usage,
                           const GLvoid* data,
                           GLenum error);

  static void SetTexParameteriWithExpectations(::gl::MockGLInterface* gl,
                                               MockErrorState* error_state,
                                               TextureManager* manager,
                                               TextureRef* texture_ref,
                                               GLenum pname,
                                               GLint value,
                                               GLenum error);

  static void SetShaderStates(
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
      OptionsAffectingCompilationString* options_affecting_compilation);

  static void SetShaderStates(::gl::MockGLInterface* gl,
                              Shader* shader,
                              bool valid);

  static void SetShaderStates(::gl::MockGLInterface* gl,
                              Shader* shader,
                              bool valid,
                              const std::string& options_affecting_compilation);

  static sh::Attribute ConstructAttribute(
      GLenum type, GLint array_size, GLenum precision,
      bool static_use, const std::string& name);
  static sh::Uniform ConstructUniform(
      GLenum type, GLint array_size, GLenum precision,
      bool static_use, const std::string& name);
  static sh::Varying ConstructVarying(
      GLenum type, GLint array_size, GLenum precision,
      bool static_use, const std::string& name);
  static sh::InterfaceBlockField ConstructInterfaceBlockField(
      GLenum type,
      GLint array_size,
      GLenum precision,
      bool static_use,
      const std::string& name);
  static sh::InterfaceBlock ConstructInterfaceBlock(
      GLint array_size,
      sh::BlockLayoutType layout,
      bool is_row_major_layout,
      bool static_use,
      const std::string& name,
      const std::string& instance_name,
      const std::vector<sh::InterfaceBlockField>& fields);
  static sh::OutputVariable ConstructOutputVariable(GLenum type,
                                                    GLint array_size,
                                                    GLenum precision,
                                                    bool static_use,
                                                    const std::string& name);

 private:
  static void SetupTextureInitializationExpectations(::gl::MockGLInterface* gl,
                                                     GLenum target,
                                                     bool use_default_textures);
  static void SetupTextureDestructionExpectations(::gl::MockGLInterface* gl,
                                                  GLenum target,
                                                  bool use_default_textures);

  static std::vector<std::string> split_extensions_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEST_HELPER_H_
