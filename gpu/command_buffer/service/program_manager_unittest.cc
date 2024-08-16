// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/program_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

namespace {
const uint32_t kMaxVaryingVectors = 8;
const uint32_t kMaxDrawBuffers = 8;
const uint32_t kMaxDualSourceDrawBuffers = 8;
const uint32_t kMaxVertexAttribs = 8;

uint32_t ComputeOffset(const void* start, const void* position) {
  return static_cast<const uint8_t*>(position) -
         static_cast<const uint8_t*>(start);
}

}  // namespace anonymous

class ProgramManagerTestBase : public GpuServiceTest, public DecoderClient {
 protected:
  virtual void SetupProgramManager() {
    manager_ = std::make_unique<ProgramManager>(
        nullptr, kMaxVaryingVectors, kMaxDrawBuffers, kMaxDualSourceDrawBuffers,
        kMaxVertexAttribs, gpu_preferences_, feature_info_.get(), nullptr);
  }
  void SetUpBase(const char* gl_version,
                 const char* gl_extensions,
                 FeatureInfo* feature_info = nullptr) {
    GpuServiceTest::SetUpWithGLVersion(gl_version, gl_extensions);
    if (!feature_info)
      feature_info = new FeatureInfo();
    TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
        gl_.get(), gl_extensions, "", gl_version, feature_info->context_type());
    feature_info->InitializeForTesting();
    feature_info_ = feature_info;
    SetupProgramManager();
  }
  void SetUp() override {
    // Parameters same as GpuServiceTest::SetUp
    SetUpBase("OpenGL ES 2.0", "GL_EXT_framebuffer_object");
  }
  void TearDown() override {
    manager_->Destroy(false);
    manager_.reset();
    feature_info_ = nullptr;
    GpuServiceTest::TearDown();
  }

  void OnConsoleMessage(int32_t id, const std::string& message) override {}
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& blob) override {}
  void OnFenceSyncRelease(uint64_t release) override {}
  void OnDescheduleUntilFinished() override {}
  void OnRescheduleAfterFinished() override {}
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override {}
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override {}
  bool ShouldYield() override { return false; }

  std::unique_ptr<ProgramManager> manager_;
  GpuPreferences gpu_preferences_;
  scoped_refptr<FeatureInfo> feature_info_;
};

class ProgramManagerTest : public ProgramManagerTestBase {};

TEST_F(ProgramManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create program.
  manager_->CreateProgram(kClient1Id, kService1Id);
  // Check program got created.
  Program* program1 = manager_->GetProgram(kClient1Id);
  ASSERT_TRUE(program1 != nullptr);
  GLuint client_id = 0;
  EXPECT_TRUE(manager_->GetClientId(program1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent program.
  EXPECT_TRUE(manager_->GetProgram(kClient2Id) == nullptr);
}

TEST_F(ProgramManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  Program* program0 = manager_->CreateProgram(kClient1Id, kService1Id);
  ASSERT_TRUE(program0 != nullptr);
  // Check program got created.
  Program* program1 = manager_->GetProgram(kClient1Id);
  ASSERT_EQ(program0, program1);
  EXPECT_CALL(*gl_, DeleteProgram(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy(true);
  // Check the resources were released.
  program1 = manager_->GetProgram(kClient1Id);
  ASSERT_TRUE(program1 == nullptr);
}

TEST_F(ProgramManagerTest, DeleteBug) {
  ShaderManager shader_manager(nullptr);
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  // Check we can create program.
  scoped_refptr<Program> program1(
      manager_->CreateProgram(kClient1Id, kService1Id));
  scoped_refptr<Program> program2(
      manager_->CreateProgram(kClient2Id, kService2Id));
  // Check program got created.
  ASSERT_TRUE(program1.get());
  ASSERT_TRUE(program2.get());
  manager_->UseProgram(program1.get());
  manager_->MarkAsDeleted(&shader_manager, program1.get());
  //  Program will be deleted when last ref is released.
  EXPECT_CALL(*gl_, DeleteProgram(kService2Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_->MarkAsDeleted(&shader_manager, program2.get());
  EXPECT_TRUE(manager_->IsOwned(program1.get()));
  EXPECT_FALSE(manager_->IsOwned(program2.get()));
}

TEST_F(ProgramManagerTest, Program) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  Program* program1 = manager_->CreateProgram(kClient1Id, kService1Id);
  ASSERT_TRUE(program1);
  EXPECT_EQ(kService1Id, program1->service_id());
  EXPECT_FALSE(program1->InUse());
  EXPECT_FALSE(program1->IsValid());
  EXPECT_FALSE(program1->IsDeleted());
  EXPECT_FALSE(program1->CanLink());
  EXPECT_TRUE(program1->log_info() == nullptr);
}

class ProgramManagerWithShaderTest : public ProgramManagerTestBase {
 public:
  static const GLint kNumVertexAttribs = 16;

  static const GLuint kClientProgramId = 123;
  static const GLuint kServiceProgramId = 456;
  static const GLuint kVertexShaderClientId = 201;
  static const GLuint kFragmentShaderClientId = 202;
  static const GLuint kVertexShaderServiceId = 301;
  static const GLuint kFragmentShaderServiceId = 302;

  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;
  static const char* kAttrib4Name;
  static const GLint kAttrib1Size = 1;
  static const GLint kAttrib2Size = 1;
  static const GLint kAttrib3Size = 1;
  static const GLint kAttrib4Size = 1;
  static const GLenum kAttrib1Precision = GL_MEDIUM_FLOAT;
  static const GLenum kAttrib2Precision = GL_HIGH_FLOAT;
  static const GLenum kAttrib3Precision = GL_LOW_INT;
  static const GLenum kAttrib4Precision = GL_HIGH_FLOAT;
  static const bool kAttribStaticUse = true;
  static const GLint kAttrib1Location = 0;
  static const GLint kAttrib2Location = 1;
  static const GLint kAttrib3Location = 2;
  static const GLint kAttrib4Location = 3;
  static const GLenum kAttrib1Type = GL_FLOAT_VEC4;
  static const GLenum kAttrib2Type = GL_FLOAT_VEC2;
  static const GLenum kAttrib3Type = GL_INT_VEC3;
  static const GLenum kAttrib4Type = GL_FLOAT_MAT3x2;
  static const GLint kInvalidAttribLocation = 30;
  static const GLint kBadAttribIndex = kNumVertexAttribs;

  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform2NameWithArrayIndex;
  static const char* kUniform3Name;
  static const char* kUniform3NameWithArrayIndex;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const int kUniform1Precision = GL_LOW_FLOAT;
  static const int kUniform2Precision = GL_MEDIUM_INT;
  static const int kUniform3Precision = GL_HIGH_FLOAT;
  static const int kUniform1StaticUse = 1;
  static const int kUniform2StaticUse = 1;
  static const int kUniform3StaticUse = 1;
  static const GLint kUniform1FakeLocation = 0;  // These are hard coded
  static const GLint kUniform2FakeLocation = 1;  // to match
  static const GLint kUniform3FakeLocation = 2;  // ProgramManager.
  static const GLint kUniform1RealLocation = 11;
  static const GLint kUniform2RealLocation = 22;
  static const GLint kUniform3RealLocation = 33;
  static const GLint kUniform1DesiredLocation = -1;
  static const GLint kUniform2DesiredLocation = -1;
  static const GLint kUniform3DesiredLocation = -1;
  static const GLenum kUniform1Type = GL_FLOAT_VEC4;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

  static const char* kOutputVariable1Name;
  static const GLint kOutputVariable1Size = 0;
  static const GLenum kOutputVariable1Precision = GL_MEDIUM_FLOAT;
  static const bool kOutputVariable1StaticUse = true;
  static const GLint kOutputVariable1Location = -1;
  static const GLenum kOutputVariable1Type = GL_FLOAT_VEC4;

  static const size_t kNumAttribs;
  static const size_t kNumUniforms;

  ProgramManagerWithShaderTest() : shader_manager_(nullptr) {}

 protected:
  typedef TestHelper::AttribInfo AttribInfo;
  typedef TestHelper::UniformInfo UniformInfo;
  typedef TestHelper::ProgramOutputInfo ProgramOutputInfo;

  typedef enum {
    kVarUniform,
    kVarVarying,
    kVarAttribute,
    kVarOutput,
  } VarCategory;

  typedef struct {
    GLenum type;
    GLint size;
    GLenum precision;
    bool static_use;
    std::string name;
    VarCategory category;
  } VarInfo;

  void SetUp() override {
    // Need to be at least ES3 for UniformBlock related GL APIs.
    SetUpBase("OpenGL ES 3.0", "");
  }

  Program* SetupDefaultProgram() {
    SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                            nullptr, 0, kServiceProgramId);

    Shader* vertex_shader = shader_manager_.CreateShader(
        kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    Shader* fragment_shader =
        shader_manager_.CreateShader(
            kFragmentShaderClientId, kFragmentShaderServiceId,
            GL_FRAGMENT_SHADER);
    EXPECT_TRUE(vertex_shader != nullptr);
    EXPECT_TRUE(fragment_shader != nullptr);
    TestHelper::SetShaderStates(gl_.get(), vertex_shader, true);
    TestHelper::SetShaderStates(gl_.get(), fragment_shader, true);

    Program* program =
        manager_->CreateProgram(kClientProgramId, kServiceProgramId);
    EXPECT_TRUE(program != nullptr);

    program->AttachShader(&shader_manager_, vertex_shader);
    program->AttachShader(&shader_manager_, fragment_shader);
    program->Link(nullptr, this);
    return program;
  }

  void SetupShaderExpectations(AttribInfo* attribs,
                               size_t num_attribs,
                               UniformInfo* uniforms,
                               size_t num_uniforms,
                               ProgramOutputInfo* program_outputs,
                               size_t num_program_outputs,
                               GLuint service_id) {
    TestHelper::SetupShaderExpectationsWithVaryings(
        gl_.get(), feature_info_.get(), attribs, num_attribs, uniforms,
        num_uniforms, nullptr, 0, program_outputs, num_program_outputs,
        service_id);
  }

  // Return true if link status matches expected_link_status
  bool LinkAsExpected(Program* program,
                      bool expected_link_status,
                      ProgramOutputInfo* program_outputs = nullptr,
                      size_t num_program_outputs = 0) {
    GLuint service_id = program->service_id();
    if (expected_link_status) {
      SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                              program_outputs, num_program_outputs, service_id);
    }
    program->Link(nullptr, this);
    GLint link_status;
    program->GetProgramiv(GL_LINK_STATUS, &link_status);
    return (static_cast<bool>(link_status) == expected_link_status);
  }

  Program* SetupProgramForVariables(const VarInfo* vertex_variables,
                                    size_t vertex_variable_size,
                                    const VarInfo* fragment_variables,
                                    size_t fragment_variable_size,
                                    const int* const shader_version = nullptr) {
    // Set up shader
    AttributeMap vertex_attrib_map;
    UniformMap vertex_uniform_map;
    VaryingMap vertex_varying_map;
    OutputVariableList vertex_output_variable_list;
    for (size_t ii = 0; ii < vertex_variable_size; ++ii) {
      switch (vertex_variables[ii].category) {
        case kVarAttribute:
          vertex_attrib_map[vertex_variables[ii].name] =
              TestHelper::ConstructAttribute(
                  vertex_variables[ii].type,
                  vertex_variables[ii].size,
                  vertex_variables[ii].precision,
                  vertex_variables[ii].static_use,
                  vertex_variables[ii].name);
          break;
        case kVarUniform:
          vertex_uniform_map[vertex_variables[ii].name] =
              TestHelper::ConstructUniform(
                  vertex_variables[ii].type,
                  vertex_variables[ii].size,
                  vertex_variables[ii].precision,
                  vertex_variables[ii].static_use,
                  vertex_variables[ii].name);
          break;
        case kVarVarying:
          vertex_varying_map[vertex_variables[ii].name] =
              TestHelper::ConstructVarying(
                  vertex_variables[ii].type,
                  vertex_variables[ii].size,
                  vertex_variables[ii].precision,
                  vertex_variables[ii].static_use,
                  vertex_variables[ii].name);
          break;
        case kVarOutput:
          vertex_output_variable_list.push_back(
              TestHelper::ConstructOutputVariable(
                  vertex_variables[ii].type, vertex_variables[ii].size,
                  vertex_variables[ii].precision,
                  vertex_variables[ii].static_use, vertex_variables[ii].name));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }

    AttributeMap frag_attrib_map;
    UniformMap frag_uniform_map;
    VaryingMap frag_varying_map;
    OutputVariableList frag_output_variable_list;
    for (size_t ii = 0; ii < fragment_variable_size; ++ii) {
      switch (fragment_variables[ii].category) {
        case kVarAttribute:
          frag_attrib_map[fragment_variables[ii].name] =
              TestHelper::ConstructAttribute(
                  fragment_variables[ii].type,
                  fragment_variables[ii].size,
                  fragment_variables[ii].precision,
                  fragment_variables[ii].static_use,
                  fragment_variables[ii].name);
          break;
        case kVarUniform:
          frag_uniform_map[fragment_variables[ii].name] =
              TestHelper::ConstructUniform(
                  fragment_variables[ii].type,
                  fragment_variables[ii].size,
                  fragment_variables[ii].precision,
                  fragment_variables[ii].static_use,
                  fragment_variables[ii].name);
          break;
        case kVarVarying:
          frag_varying_map[fragment_variables[ii].name] =
              TestHelper::ConstructVarying(
                  fragment_variables[ii].type,
                  fragment_variables[ii].size,
                  fragment_variables[ii].precision,
                  fragment_variables[ii].static_use,
                  fragment_variables[ii].name);
          break;
        case kVarOutput:
          frag_output_variable_list.push_back(
              TestHelper::ConstructOutputVariable(
                  fragment_variables[ii].type, fragment_variables[ii].size,
                  fragment_variables[ii].precision,
                  fragment_variables[ii].static_use,
                  fragment_variables[ii].name));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }

    // Check we can create shader.
    Shader* vshader = shader_manager_.CreateShader(
        kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    Shader* fshader = shader_manager_.CreateShader(
        kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
    // Check shader got created.
    EXPECT_TRUE(vshader != nullptr && fshader != nullptr);
    // Set Status
    TestHelper::SetShaderStates(gl_.get(), vshader, true, nullptr, nullptr,
                                shader_version, &vertex_attrib_map,
                                &vertex_uniform_map, &vertex_varying_map,
                                nullptr, &vertex_output_variable_list, nullptr);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                shader_version, &frag_attrib_map,
                                &frag_uniform_map, &frag_varying_map, nullptr,
                                &frag_output_variable_list, nullptr);

    // Set up program
    Program* program =
        manager_->CreateProgram(kClientProgramId, kServiceProgramId);
    EXPECT_TRUE(program != nullptr);
    EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
    EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
    return program;
  }

  void TearDown() override {
    shader_manager_.Destroy(false);
    ProgramManagerTestBase::TearDown();
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];
  ShaderManager shader_manager_;
};

ProgramManagerWithShaderTest::AttribInfo
    ProgramManagerWithShaderTest::kAttribs[] = {
        {
            kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location,
        },
        {
            kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location,
        },
        {
            kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location,
        },
        {
            kAttrib4Name, kAttrib4Size, kAttrib4Type, kAttrib4Location,
        },
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint ProgramManagerWithShaderTest::kNumVertexAttribs;
const GLuint ProgramManagerWithShaderTest::kClientProgramId;
const GLuint ProgramManagerWithShaderTest::kServiceProgramId;
const GLuint ProgramManagerWithShaderTest::kVertexShaderClientId;
const GLuint ProgramManagerWithShaderTest::kFragmentShaderClientId;
const GLuint ProgramManagerWithShaderTest::kVertexShaderServiceId;
const GLuint ProgramManagerWithShaderTest::kFragmentShaderServiceId;
const GLint ProgramManagerWithShaderTest::kAttrib1Size;
const GLint ProgramManagerWithShaderTest::kAttrib2Size;
const GLint ProgramManagerWithShaderTest::kAttrib3Size;
const GLint ProgramManagerWithShaderTest::kAttrib4Size;
const GLint ProgramManagerWithShaderTest::kAttrib1Location;
const GLint ProgramManagerWithShaderTest::kAttrib2Location;
const GLint ProgramManagerWithShaderTest::kAttrib3Location;
const GLint ProgramManagerWithShaderTest::kAttrib4Location;
const GLenum ProgramManagerWithShaderTest::kAttrib1Type;
const GLenum ProgramManagerWithShaderTest::kAttrib2Type;
const GLenum ProgramManagerWithShaderTest::kAttrib3Type;
const GLenum ProgramManagerWithShaderTest::kAttrib4Type;
const GLint ProgramManagerWithShaderTest::kInvalidAttribLocation;
const GLint ProgramManagerWithShaderTest::kBadAttribIndex;
const GLint ProgramManagerWithShaderTest::kUniform1Size;
const GLint ProgramManagerWithShaderTest::kUniform2Size;
const GLint ProgramManagerWithShaderTest::kUniform3Size;
const GLint ProgramManagerWithShaderTest::kUniform1FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform2FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform3FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform1RealLocation;
const GLint ProgramManagerWithShaderTest::kUniform2RealLocation;
const GLint ProgramManagerWithShaderTest::kUniform3RealLocation;
const GLint ProgramManagerWithShaderTest::kUniform1DesiredLocation;
const GLint ProgramManagerWithShaderTest::kUniform2DesiredLocation;
const GLint ProgramManagerWithShaderTest::kUniform3DesiredLocation;
const GLenum ProgramManagerWithShaderTest::kUniform1Type;
const GLenum ProgramManagerWithShaderTest::kUniform2Type;
const GLenum ProgramManagerWithShaderTest::kUniform3Type;
const GLint ProgramManagerWithShaderTest::kInvalidUniformLocation;
const GLint ProgramManagerWithShaderTest::kBadUniformIndex;
#endif

const size_t ProgramManagerWithShaderTest::kNumAttribs =
    std::size(ProgramManagerWithShaderTest::kAttribs);

ProgramManagerWithShaderTest::UniformInfo
    ProgramManagerWithShaderTest::kUniforms[] = {
  { kUniform1Name,
    kUniform1Size,
    kUniform1Type,
    kUniform1FakeLocation,
    kUniform1RealLocation,
    kUniform1DesiredLocation,
    kUniform1Name,
  },
  { kUniform2Name,
    kUniform2Size,
    kUniform2Type,
    kUniform2FakeLocation,
    kUniform2RealLocation,
    kUniform2DesiredLocation,
    kUniform2NameWithArrayIndex,
  },
  { kUniform3Name,
    kUniform3Size,
    kUniform3Type,
    kUniform3FakeLocation,
    kUniform3RealLocation,
    kUniform3DesiredLocation,
    kUniform3NameWithArrayIndex,
  },
};

const size_t ProgramManagerWithShaderTest::kNumUniforms =
    std::size(ProgramManagerWithShaderTest::kUniforms);

const char* ProgramManagerWithShaderTest::kAttrib1Name = "attrib1";
const char* ProgramManagerWithShaderTest::kAttrib2Name = "attrib2";
const char* ProgramManagerWithShaderTest::kAttrib3Name = "attrib3";
const char* ProgramManagerWithShaderTest::kAttrib4Name = "attrib4";
const char* ProgramManagerWithShaderTest::kUniform1Name = "uniform1";
const char* ProgramManagerWithShaderTest::kUniform2Name = "uniform2";
const char* ProgramManagerWithShaderTest::kUniform2NameWithArrayIndex =
    "uniform2[0]";
const char* ProgramManagerWithShaderTest::kUniform3Name = "uniform3";
const char* ProgramManagerWithShaderTest::kUniform3NameWithArrayIndex =
    "uniform3[0]";
const char* ProgramManagerWithShaderTest::kOutputVariable1Name = "outputVar1";

TEST_F(ProgramManagerWithShaderTest, GetAttribInfos) {
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  const Program::AttribInfoVector& infos =
      program->GetAttribInfos();
  ASSERT_EQ(kNumAttribs, infos.size());
  for (size_t ii = 0; ii < kNumAttribs; ++ii) {
    const Program::VertexAttrib& info = infos[ii];
    const AttribInfo& expected = kAttribs[ii];
    EXPECT_EQ(expected.size, info.size);
    EXPECT_EQ(expected.type, info.type);
    EXPECT_EQ(expected.location, info.location);
    EXPECT_STREQ(expected.name, info.name.c_str());
  }
}

TEST_F(ProgramManagerWithShaderTest, GetAttribInfo) {
  const GLint kValidIndex = 1;
  const GLint kInvalidIndex = 1000;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  const Program::VertexAttrib* info =
      program->GetAttribInfo(kValidIndex);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kAttrib2Size, info->size);
  EXPECT_EQ(kAttrib2Type, info->type);
  EXPECT_EQ(kAttrib2Location, info->location);
  EXPECT_STREQ(kAttrib2Name, info->name.c_str());
  EXPECT_TRUE(program->GetAttribInfo(kInvalidIndex) == nullptr);
}

TEST_F(ProgramManagerWithShaderTest, GetAttribInfoByLocation) {
  const GLint kInvalidLocation = 1000;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);

  // attrib2 is a vec2, takes 1 location
  const Program::VertexAttrib* expected_info = program->GetAttribInfo(1);
  EXPECT_EQ(1u, expected_info->location_count);
  const Program::VertexAttrib* info =
      program->GetAttribInfoByLocation(kAttrib2Location);
  EXPECT_EQ(expected_info, info);

  // attrib4 is a mat3x2, takes 3 locations (1 per column)
  expected_info = program->GetAttribInfo(3);
  EXPECT_EQ(3u, expected_info->location_count);
  info = program->GetAttribInfoByLocation(kAttrib4Location);
  EXPECT_EQ(expected_info, info);
  info = program->GetAttribInfoByLocation(kAttrib4Location + 1);
  EXPECT_EQ(expected_info, info);
  info = program->GetAttribInfoByLocation(kAttrib4Location + 2);
  EXPECT_EQ(expected_info, info);

  EXPECT_TRUE(program->GetAttribInfoByLocation(kInvalidLocation) == nullptr);
}

TEST_F(ProgramManagerWithShaderTest, GetAttribLocation) {
  const char* kInvalidName = "foo";
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  EXPECT_EQ(kAttrib2Location, program->GetAttribLocation(kAttrib2Name));
  EXPECT_EQ(-1, program->GetAttribLocation(kInvalidName));
}

TEST_F(ProgramManagerWithShaderTest, VertexArrayMasks) {
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);

  std::vector<uint32_t> active_mask = program->vertex_input_active_mask();
  ASSERT_EQ(1u, active_mask.size());
  uint32_t expected = 0x3 << 0 |              // attrib1
                      0x3 << 2 |              // attrib2
                      0x3 << 4 |              // attrib3
                      (0x3 * 0b010101) << 6;  // attrib4
  EXPECT_EQ(expected, active_mask[0]);

  std::vector<uint32_t> base_type_mask = program->vertex_input_base_type_mask();
  ASSERT_EQ(1u, base_type_mask.size());
  expected = SHADER_VARIABLE_FLOAT << 0 |              // attrib1
             SHADER_VARIABLE_FLOAT << 2 |              // attrib2
             SHADER_VARIABLE_INT << 4 |                // attrib3
             (SHADER_VARIABLE_FLOAT * 0b010101) << 6;  // attrib4
  EXPECT_EQ(expected, base_type_mask[0]);
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfo) {
  const GLint kInvalidIndex = 1000;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  const Program::UniformInfo* info =
      program->GetUniformInfo(0);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kUniform1Size, info->size);
  EXPECT_EQ(kUniform1Type, info->type);
  EXPECT_EQ(kUniform1RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform1Name, info->name.c_str());
  info = program->GetUniformInfo(1);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kUniform2Size, info->size);
  EXPECT_EQ(kUniform2Type, info->type);
  EXPECT_EQ(kUniform2RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform2NameWithArrayIndex, info->name.c_str());
  info = program->GetUniformInfo(2);
  // We emulate certain OpenGL drivers by supplying the name without
  // the array spec. Our implementation should correctly add the required spec.
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kUniform3Size, info->size);
  EXPECT_EQ(kUniform3Type, info->type);
  EXPECT_EQ(kUniform3RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform3NameWithArrayIndex, info->name.c_str());
  EXPECT_TRUE(program->GetUniformInfo(kInvalidIndex) == nullptr);
}

TEST_F(ProgramManagerWithShaderTest, AttachDetachShader) {
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_FALSE(program->CanLink());
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_FALSE(program->CanLink());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program->CanLink());
  program->DetachShader(&shader_manager_, vshader);
  EXPECT_FALSE(program->CanLink());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->CanLink());
  program->DetachShader(&shader_manager_, fshader);
  EXPECT_FALSE(program->CanLink());
  EXPECT_FALSE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_FALSE(program->CanLink());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program->CanLink());
  TestHelper::SetShaderStates(gl_.get(), vshader, false);
  EXPECT_FALSE(program->CanLink());
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  EXPECT_TRUE(program->CanLink());
  TestHelper::SetShaderStates(gl_.get(), fshader, false);
  EXPECT_FALSE(program->CanLink());
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  EXPECT_TRUE(program->CanLink());
  EXPECT_TRUE(program->IsShaderAttached(fshader));
  program->DetachShader(&shader_manager_, fshader);
  EXPECT_FALSE(program->IsShaderAttached(fshader));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformFakeLocation) {
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  // Emulate the situation that uniform3[1] isn't used and optimized out by
  // a driver, so it's location is -1.
  Program::UniformInfo* uniform = const_cast<Program::UniformInfo*>(
      program->GetUniformInfo(2));
  ASSERT_TRUE(uniform != nullptr && kUniform3Size == 2);
  EXPECT_EQ(kUniform3Size, uniform->size);
  uniform->element_locations[1] = -1;
  EXPECT_EQ(kUniform1FakeLocation,
            program->GetUniformFakeLocation(kUniform1Name));
  EXPECT_EQ(kUniform2FakeLocation,
            program->GetUniformFakeLocation(kUniform2Name));
  EXPECT_EQ(kUniform3FakeLocation,
            program->GetUniformFakeLocation(kUniform3Name));
  // Check we can get uniform2 as "uniform2" even though the name is
  // "uniform2[0]"
  EXPECT_EQ(kUniform2FakeLocation,
            program->GetUniformFakeLocation("uniform2"));
  // Check we can get uniform3 as "uniform3[0]" even though we simulated GL
  // returning "uniform3"
  EXPECT_EQ(kUniform3FakeLocation,
            program->GetUniformFakeLocation(kUniform3NameWithArrayIndex));
  // Check that we can get the locations of the array elements > 1
  EXPECT_EQ(ProgramManager::MakeFakeLocation(kUniform2FakeLocation, 1),
            program->GetUniformFakeLocation("uniform2[1]"));
  EXPECT_EQ(ProgramManager::MakeFakeLocation(kUniform2FakeLocation, 2),
            program->GetUniformFakeLocation("uniform2[2]"));
  EXPECT_EQ(-1, program->GetUniformFakeLocation("uniform2[3]"));
  EXPECT_EQ(-1, program->GetUniformFakeLocation("uniform3[1]"));
  EXPECT_EQ(-1, program->GetUniformFakeLocation("uniform3[2]"));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfoByFakeLocation) {
  const GLint kInvalidLocation = 1234;
  const Program::UniformInfo* info;
  const Program* program = SetupDefaultProgram();
  GLint real_location = -1;
  GLint array_index = -1;
  ASSERT_TRUE(program != nullptr);
  info = program->GetUniformInfoByFakeLocation(
      kUniform2FakeLocation, &real_location, &array_index);
  EXPECT_EQ(kUniform2RealLocation, real_location);
  EXPECT_EQ(0, array_index);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kUniform2Type, info->type);
  real_location = -1;
  array_index = -1;
  info = program->GetUniformInfoByFakeLocation(
      kInvalidLocation, &real_location, &array_index);
  EXPECT_TRUE(info == nullptr);
  EXPECT_EQ(-1, real_location);
  EXPECT_EQ(-1, array_index);
  GLint loc = program->GetUniformFakeLocation("uniform2[2]");
  info = program->GetUniformInfoByFakeLocation(
      loc, &real_location, &array_index);
  ASSERT_TRUE(info != nullptr);
  EXPECT_EQ(kUniform2RealLocation + 2 * 2, real_location);
  EXPECT_EQ(2, array_index);
}

// Ensure that when GL drivers correctly return gl_DepthRange, or other
// builtin uniforms, our implementation passes them back to the client.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsGLUnderscoreUniform) {
  static const char* kUniform2Name = "gl_longNameWeCanCheckFor";
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
      {
       kUniform1Name,
       kUniform1Size,
       kUniform1Type,
       kUniform1FakeLocation,
       kUniform1RealLocation,
       kUniform1DesiredLocation,
       kUniform1Name,
      },
      {
       kUniform2Name,
       kUniform2Size,
       kUniform2Type,
       kUniform2FakeLocation,
       -1,
       kUniform2DesiredLocation,
       kUniform2NameWithArrayIndex,
      },
      {
       kUniform3Name,
       kUniform3Size,
       kUniform3Type,
       kUniform3FakeLocation,
       kUniform3RealLocation,
       kUniform3DesiredLocation,
       kUniform3NameWithArrayIndex,
      },
  };
  const size_t kNumUniforms = std::size(kUniforms);
  SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                          nullptr, 0, kServiceProgramId);
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  program->Link(nullptr, this);
  GLint value = 0;
  program->GetProgramiv(GL_ACTIVE_ATTRIBUTES, &value);
  EXPECT_EQ(4, value);
  // Check that we didn't skip the "gl_" uniform.
  program->GetProgramiv(GL_ACTIVE_UNIFORMS, &value);
  EXPECT_EQ(3, value);
  // Check that our max length adds room for the array spec and is as long
  // as the "gl_" uniform we did not skip.
  program->GetProgramiv(GL_ACTIVE_UNIFORM_MAX_LENGTH, &value);
  EXPECT_EQ(strlen(kUniform2Name) + 4, static_cast<size_t>(value));
  // Verify the uniform has a "real" location of -1
  const auto* info = program->GetUniformInfo(kUniform2FakeLocation);
  EXPECT_EQ(-1, info->element_locations[0]);
}

// Test the bug comparing similar array names is fixed.
TEST_F(ProgramManagerWithShaderTest, SimilarArrayNames) {
  static const char* kUniform2Name = "u_nameLong[0]";
  static const char* kUniform3Name = "u_name[0]";
  static const GLint kUniform2Size = 2;
  static const GLint kUniform3Size = 2;
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2Name,
    },
    { kUniform3Name,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3Name,
    },
  };
  const size_t kNumUniforms = std::size(kUniforms);
  SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                          nullptr, 0, kServiceProgramId);
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  program->Link(nullptr, this);

  // Check that we get the correct locations.
  EXPECT_EQ(kUniform2FakeLocation,
            program->GetUniformFakeLocation(kUniform2Name));
  EXPECT_EQ(kUniform3FakeLocation,
            program->GetUniformFakeLocation(kUniform3Name));
}

// Some GL drivers incorrectly return the wrong type. For example they return
// GL_FLOAT_VEC2 when they should return GL_FLOAT_MAT2. Check we handle this.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsWrongTypeInfo) {
  static GLenum kAttrib2BadType = GL_FLOAT_VEC2;
  static GLenum kAttrib2GoodType = GL_FLOAT_MAT2;
  static GLenum kUniform2BadType = GL_FLOAT_VEC3;
  static GLenum kUniform2GoodType = GL_FLOAT_MAT3;
  AttributeMap attrib_map;
  UniformMap uniform_map;
  VaryingMap varying_map;
  OutputVariableList output_variable_list;
  attrib_map[kAttrib1Name] = TestHelper::ConstructAttribute(
      kAttrib1Type, kAttrib1Size, kAttrib1Precision,
      kAttribStaticUse, kAttrib1Name);
  attrib_map[kAttrib2Name] = TestHelper::ConstructAttribute(
      kAttrib2GoodType, kAttrib2Size, kAttrib2Precision,
      kAttribStaticUse, kAttrib2Name);
  attrib_map[kAttrib3Name] = TestHelper::ConstructAttribute(
      kAttrib3Type, kAttrib3Size, kAttrib3Precision,
      kAttribStaticUse, kAttrib3Name);
  uniform_map[kUniform1Name] = TestHelper::ConstructUniform(
      kUniform1Type, kUniform1Size, kUniform1Precision,
      kUniform1StaticUse, kUniform1Name);
  uniform_map[kUniform2Name] = TestHelper::ConstructUniform(
      kUniform2GoodType, kUniform2Size, kUniform2Precision,
      kUniform2StaticUse, kUniform2Name);
  uniform_map[kUniform3Name] = TestHelper::ConstructUniform(
      kUniform3Type, kUniform3Size, kUniform3Precision,
      kUniform3StaticUse, kUniform3Name);
  output_variable_list.push_back(TestHelper::ConstructOutputVariable(
      kOutputVariable1Type, kOutputVariable1Size, kOutputVariable1Precision,
      kOutputVariable1StaticUse, kOutputVariable1Name));

  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true, nullptr, nullptr,
                              nullptr, &attrib_map, &uniform_map, &varying_map,
                              nullptr, &output_variable_list, nullptr);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                              nullptr, &attrib_map, &uniform_map, &varying_map,
                              nullptr, &output_variable_list, nullptr);
  static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
    { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
    { kAttrib2Name, kAttrib2Size, kAttrib2BadType, kAttrib2Location, },
    { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
  };
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2BadType,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2NameWithArrayIndex,
    },
    { kUniform3Name,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3NameWithArrayIndex,
    },
  };
  static ProgramManagerWithShaderTest::ProgramOutputInfo kProgramOutputs[] = {
      {
          kOutputVariable1Name, kOutputVariable1Size, kOutputVariable1Type,
          7,  // color_name
          0,  // index
      },
  };
  const size_t kNumAttribs = std::size(kAttribs);
  const size_t kNumUniforms = std::size(kUniforms);
  const size_t kNumProgramOutputs = std::size(kProgramOutputs);
  SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                          kProgramOutputs, kNumProgramOutputs,
                          kServiceProgramId);
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  program->Link(nullptr, this);
  // Check that we got the good type, not the bad.
  // Check Attribs
  for (unsigned index = 0; index < kNumAttribs; ++index) {
    const Program::VertexAttrib* attrib_info =
        program->GetAttribInfo(index);
    ASSERT_TRUE(attrib_info != nullptr);
    size_t pos = attrib_info->name.find_first_of("[.");
    std::string top_name;
    if (pos == std::string::npos)
      top_name = attrib_info->name;
    else
      top_name = attrib_info->name.substr(0, pos);
    AttributeMap::const_iterator it = attrib_map.find(top_name);
    ASSERT_TRUE(it != attrib_map.end());
    const sh::ShaderVariable* info;
    std::string original_name;
    EXPECT_TRUE(it->second.findInfoByMappedName(
        attrib_info->name, &info, &original_name));
    EXPECT_EQ(info->type, attrib_info->type);
    EXPECT_EQ(static_cast<GLint>(info->getOutermostArraySize()),
              attrib_info->size);
    EXPECT_EQ(original_name, attrib_info->name);
  }
  // Check Uniforms
  for (unsigned index = 0; index < kNumUniforms; ++index) {
    const Program::UniformInfo* uniform_info = program->GetUniformInfo(index);
    ASSERT_TRUE(uniform_info != nullptr);
    size_t pos = uniform_info->name.find_first_of("[.");
    std::string top_name;
    if (pos == std::string::npos)
      top_name = uniform_info->name;
    else
      top_name = uniform_info->name.substr(0, pos);
    UniformMap::const_iterator it = uniform_map.find(top_name);
    ASSERT_TRUE(it != uniform_map.end());
    const sh::ShaderVariable* info;
    std::string original_name;
    EXPECT_TRUE(it->second.findInfoByMappedName(
        uniform_info->name, &info, &original_name));
    EXPECT_EQ(info->type, uniform_info->type);
    EXPECT_EQ(static_cast<GLint>(info->getOutermostArraySize()),
              uniform_info->size);
    EXPECT_EQ(original_name, uniform_info->name);
  }
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount) {
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_FALSE(program->CanLink());
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program->CanLink());
  EXPECT_FALSE(program->InUse());
  EXPECT_FALSE(program->IsDeleted());
  manager_->UseProgram(program);
  EXPECT_TRUE(program->InUse());
  manager_->UseProgram(program);
  EXPECT_TRUE(program->InUse());
  manager_->MarkAsDeleted(&shader_manager_, program);
  EXPECT_TRUE(program->IsDeleted());
  Program* info2 = manager_->GetProgram(kClientProgramId);
  EXPECT_EQ(program, info2);
  manager_->UnuseProgram(&shader_manager_, program);
  EXPECT_TRUE(program->InUse());
  // this should delete the info.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_->UnuseProgram(&shader_manager_, program);
  info2 = manager_->GetProgram(kClientProgramId);
  EXPECT_TRUE(info2 == nullptr);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount2) {
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_FALSE(program->CanLink());
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program->CanLink());
  EXPECT_FALSE(program->InUse());
  EXPECT_FALSE(program->IsDeleted());
  manager_->UseProgram(program);
  EXPECT_TRUE(program->InUse());
  manager_->UseProgram(program);
  EXPECT_TRUE(program->InUse());
  manager_->UnuseProgram(&shader_manager_, program);
  EXPECT_TRUE(program->InUse());
  manager_->UnuseProgram(&shader_manager_, program);
  EXPECT_FALSE(program->InUse());
  Program* info2 = manager_->GetProgram(kClientProgramId);
  EXPECT_EQ(program, info2);
  // this should delete the program.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_->MarkAsDeleted(&shader_manager_, program);
  info2 = manager_->GetProgram(kClientProgramId);
  EXPECT_TRUE(info2 == nullptr);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetProgramInfo) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  program->GetProgramInfo(manager_.get(), &bucket);
  ProgramInfoHeader* header =
      bucket.GetDataAs<ProgramInfoHeader*>(0, sizeof(ProgramInfoHeader));
  ASSERT_TRUE(header != nullptr);
  EXPECT_EQ(1u, header->link_status);
  EXPECT_EQ(std::size(kAttribs), header->num_attribs);
  EXPECT_EQ(std::size(kUniforms), header->num_uniforms);
  const ProgramInput* inputs = bucket.GetDataAs<const ProgramInput*>(
      sizeof(*header),
      sizeof(ProgramInput) * (header->num_attribs + header->num_uniforms));
  ASSERT_TRUE(inputs != nullptr);
  const ProgramInput* input = inputs;
  // TODO(gman): Don't assume these are in order.
  for (uint32_t ii = 0; ii < header->num_attribs; ++ii) {
    const AttribInfo& expected = kAttribs[ii];
    EXPECT_EQ(expected.size, input->size);
    EXPECT_EQ(expected.type, input->type);
    const int32_t* location = bucket.GetDataAs<const int32_t*>(
        input->location_offset, sizeof(int32_t));
    ASSERT_TRUE(location != nullptr);
    EXPECT_EQ(expected.location, *location);
    const char* name_buf = bucket.GetDataAs<const char*>(
        input->name_offset, input->name_length);
    ASSERT_TRUE(name_buf != nullptr);
    std::string name(name_buf, input->name_length);
    EXPECT_STREQ(expected.name, name.c_str());
    ++input;
  }
  // TODO(gman): Don't assume these are in order.
  for (uint32_t ii = 0; ii < header->num_uniforms; ++ii) {
    const UniformInfo& expected = kUniforms[ii];
    EXPECT_EQ(expected.size, input->size);
    EXPECT_EQ(expected.type, input->type);
    const int32_t* locations = bucket.GetDataAs<const int32_t*>(
        input->location_offset, sizeof(int32_t) * input->size);
    ASSERT_TRUE(locations != nullptr);
    for (int32_t jj = 0; jj < input->size; ++jj) {
      EXPECT_EQ(
          ProgramManager::MakeFakeLocation(expected.fake_location, jj),
          locations[jj]);
    }
    const char* name_buf = bucket.GetDataAs<const char*>(
        input->name_offset, input->name_length);
    ASSERT_TRUE(name_buf != nullptr);
    std::string name(name_buf, input->name_length);
    EXPECT_STREQ(expected.good_name, name.c_str());
    ++input;
  }
  EXPECT_EQ(header->num_attribs + header->num_uniforms,
            static_cast<uint32_t>(input - inputs));
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetUniformBlocksNone) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  // The program's previous link failed.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetUniformBlocks(&bucket));
  EXPECT_EQ(sizeof(UniformBlocksHeader), bucket.size());
  UniformBlocksHeader* header =
      bucket.GetDataAs<UniformBlocksHeader*>(0, sizeof(UniformBlocksHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniform_blocks);
  // Zero uniform blocks.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORM_BLOCKS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetUniformBlocks(&bucket));
  EXPECT_EQ(sizeof(UniformBlocksHeader), bucket.size());
  header =
      bucket.GetDataAs<UniformBlocksHeader*>(0, sizeof(UniformBlocksHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniform_blocks);
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetUniformBlocksValid) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  struct Data {
    UniformBlocksHeader header;
    UniformBlockInfo entry[2];
    char name0[4];
    uint32_t indices0[2];
    char name1[8];
    uint32_t indices1[1];
  };
  Data data;
  // The names needs to be of size 4*k-1 to avoid padding in the struct Data.
  // This is a testing only problem.
  const char* kName[] = { "cow", "chicken" };
  const uint32_t kIndices0[] = { 1, 2 };
  const uint32_t kIndices1[] = { 3 };
  const uint32_t* kIndices[] = { kIndices0, kIndices1 };
  data.header.num_uniform_blocks = 2;
  data.entry[0].binding = 0;
  data.entry[0].data_size = 8;
  data.entry[0].name_offset = ComputeOffset(&data, data.name0);
  data.entry[0].name_length = std::size(data.name0);
  data.entry[0].active_uniforms = std::size(data.indices0);
  data.entry[0].active_uniform_offset = ComputeOffset(&data, data.indices0);
  data.entry[0].referenced_by_vertex_shader = static_cast<uint32_t>(true);
  data.entry[0].referenced_by_fragment_shader = static_cast<uint32_t>(false);
  data.entry[1].binding = 1;
  data.entry[1].data_size = 4;
  data.entry[1].name_offset = ComputeOffset(&data, data.name1);
  data.entry[1].name_length = std::size(data.name1);
  data.entry[1].active_uniforms = std::size(data.indices1);
  data.entry[1].active_uniform_offset = ComputeOffset(&data, data.indices1);
  data.entry[1].referenced_by_vertex_shader = static_cast<uint32_t>(false);
  data.entry[1].referenced_by_fragment_shader = static_cast<uint32_t>(true);
  memcpy(data.name0, kName[0], std::size(data.name0));
  data.indices0[0] = kIndices[0][0];
  data.indices0[1] = kIndices[0][1];
  memcpy(data.name1, kName[1], std::size(data.name1));
  data.indices1[0] = kIndices[1][0];

  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORM_BLOCKS, _))
      .WillOnce(SetArgPointee<2>(data.header.num_uniform_blocks))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, _))
      .WillOnce(SetArgPointee<2>(
          1 + std::max(strlen(kName[0]), strlen(kName[1]))))
      .RetiresOnSaturation();
  for (uint32_t ii = 0; ii < data.header.num_uniform_blocks; ++ii) {
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii, GL_UNIFORM_BLOCK_BINDING, _))
        .WillOnce(SetArgPointee<3>(data.entry[ii].binding))
        .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii, GL_UNIFORM_BLOCK_DATA_SIZE, _))
        .WillOnce(SetArgPointee<3>(data.entry[ii].data_size))
        .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii, GL_UNIFORM_BLOCK_NAME_LENGTH, _))
        .WillOnce(SetArgPointee<3>(data.entry[ii].name_length))
        .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockName(
                    kServiceProgramId, ii, data.entry[ii].name_length, _, _))
          .WillOnce(DoAll(
              SetArgPointee<3>(strlen(kName[ii])),
              SetArrayArgument<4>(
                  kName[ii], kName[ii] + data.entry[ii].name_length)))
          .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, _))
        .WillOnce(SetArgPointee<3>(data.entry[ii].active_uniforms))
        .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii,
                    GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, _))
        .WillOnce(SetArgPointee<3>(data.entry[ii].referenced_by_vertex_shader))
        .RetiresOnSaturation();
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii,
                    GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, _))
        .WillOnce(SetArgPointee<3>(
            data.entry[ii].referenced_by_fragment_shader))
        .RetiresOnSaturation();
  }
  for (uint32_t ii = 0; ii < data.header.num_uniform_blocks; ++ii) {
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformBlockiv(
                    kServiceProgramId, ii,
                    GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, _))
        .WillOnce(SetArrayArgument<3>(
            kIndices[ii], kIndices[ii] + data.entry[ii].active_uniforms))
        .RetiresOnSaturation();
  }
  program->GetUniformBlocks(&bucket);
  EXPECT_EQ(sizeof(Data), bucket.size());
  Data* bucket_data = bucket.GetDataAs<Data*>(0, sizeof(Data));
  EXPECT_TRUE(bucket_data != nullptr);
  EXPECT_EQ(0, memcmp(&data, bucket_data, sizeof(Data)));
}

TEST_F(ProgramManagerWithShaderTest,
       ProgramInfoGetTransformFeedbackVaryingsNone) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  // The program's previous link failed.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                           _))
      .WillOnce(SetArgPointee<2>(GL_INTERLEAVED_ATTRIBS))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetTransformFeedbackVaryings(&bucket));
  EXPECT_EQ(sizeof(TransformFeedbackVaryingsHeader), bucket.size());
  TransformFeedbackVaryingsHeader* header =
      bucket.GetDataAs<TransformFeedbackVaryingsHeader*>(
          0, sizeof(TransformFeedbackVaryingsHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_transform_feedback_varyings);
  EXPECT_EQ(static_cast<uint32_t>(GL_INTERLEAVED_ATTRIBS),
            header->transform_feedback_buffer_mode);
  // Zero transform feedback blocks.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                           _))
      .WillOnce(SetArgPointee<2>(GL_SEPARATE_ATTRIBS))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(
                  kServiceProgramId, GL_TRANSFORM_FEEDBACK_VARYINGS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetTransformFeedbackVaryings(&bucket));
  EXPECT_EQ(sizeof(TransformFeedbackVaryingsHeader), bucket.size());
  header = bucket.GetDataAs<TransformFeedbackVaryingsHeader*>(
      0, sizeof(TransformFeedbackVaryingsHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(GL_SEPARATE_ATTRIBS),
            header->transform_feedback_buffer_mode);
  EXPECT_EQ(0u, header->num_transform_feedback_varyings);
}

TEST_F(ProgramManagerWithShaderTest,
       ProgramInfoGetTransformFeedbackVaryingsValid) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  struct Data {
    TransformFeedbackVaryingsHeader header;
    TransformFeedbackVaryingInfo entry[2];
    char name0[4];
    char name1[8];
  };
  Data data;
  // The names needs to be of size 4*k-1 to avoid padding in the struct Data.
  // This is a testing only problem.
  const char* kName[] = { "cow", "chicken" };
  data.header.transform_feedback_buffer_mode = GL_INTERLEAVED_ATTRIBS;
  data.header.num_transform_feedback_varyings = 2;
  data.entry[0].size = 1;
  data.entry[0].type = GL_FLOAT_VEC2;
  data.entry[0].name_offset = ComputeOffset(&data, data.name0);
  data.entry[0].name_length = std::size(data.name0);
  data.entry[1].size = 2;
  data.entry[1].type = GL_FLOAT;
  data.entry[1].name_offset = ComputeOffset(&data, data.name1);
  data.entry[1].name_length = std::size(data.name1);
  memcpy(data.name0, kName[0], std::size(data.name0));
  memcpy(data.name1, kName[1], std::size(data.name1));

  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                           _))
      .WillOnce(SetArgPointee<2>(GL_INTERLEAVED_ATTRIBS))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(
                  kServiceProgramId, GL_TRANSFORM_FEEDBACK_VARYINGS, _))
      .WillOnce(SetArgPointee<2>(data.header.num_transform_feedback_varyings))
      .RetiresOnSaturation();
  GLsizei max_length = 1 + std::max(strlen(kName[0]), strlen(kName[1]));
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, _))
      .WillOnce(SetArgPointee<2>(max_length))
      .RetiresOnSaturation();
  for (uint32_t ii = 0; ii < data.header.num_transform_feedback_varyings;
       ++ii) {
    EXPECT_CALL(*(gl_.get()),
                GetTransformFeedbackVarying(
                    kServiceProgramId, ii, max_length, _, _, _, _))
        .WillOnce(DoAll(
            SetArgPointee<3>(data.entry[ii].name_length - 1),
            SetArgPointee<4>(data.entry[ii].size),
            SetArgPointee<5>(data.entry[ii].type),
            SetArrayArgument<6>(
                kName[ii], kName[ii] + data.entry[ii].name_length)))
        .RetiresOnSaturation();
  }
  program->GetTransformFeedbackVaryings(&bucket);
  EXPECT_EQ(sizeof(Data), bucket.size());
  Data* bucket_data = bucket.GetDataAs<Data*>(0, sizeof(Data));
  EXPECT_TRUE(bucket_data != nullptr);
  EXPECT_EQ(0, memcmp(&data, bucket_data, sizeof(Data)));
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetUniformsES3None) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  // The program's previous link failed.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetUniformsES3(&bucket));
  EXPECT_EQ(sizeof(UniformsES3Header), bucket.size());
  UniformsES3Header* header =
      bucket.GetDataAs<UniformsES3Header*>(0, sizeof(UniformsES3Header));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniforms);
  // Zero uniform blocks.
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(program->GetUniformsES3(&bucket));
  EXPECT_EQ(sizeof(UniformsES3Header), bucket.size());
  header =
      bucket.GetDataAs<UniformsES3Header*>(0, sizeof(UniformsES3Header));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniforms);
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetUniformsES3Valid) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  struct Data {
    UniformsES3Header header;
    UniformES3Info entry[2];
  };
  Data data;
  const GLint kBlockIndex[] = { -1, 2 };
  const GLint kOffset[] = { 3, 4 };
  const GLint kArrayStride[] = { 7, 8 };
  const GLint kMatrixStride[] = { 9, 10 };
  const GLint kIsRowMajor[] = { 0, 1 };
  data.header.num_uniforms = 2;
  for (uint32_t ii = 0; ii < data.header.num_uniforms; ++ii) {
    data.entry[ii].block_index = kBlockIndex[ii];
    data.entry[ii].offset = kOffset[ii];
    data.entry[ii].array_stride = kArrayStride[ii];
    data.entry[ii].matrix_stride = kMatrixStride[ii];
    data.entry[ii].is_row_major = kIsRowMajor[ii];
  }

  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(data.header.num_uniforms))
      .RetiresOnSaturation();

  const GLenum kPname[] = {
    GL_UNIFORM_BLOCK_INDEX,
    GL_UNIFORM_OFFSET,
    GL_UNIFORM_ARRAY_STRIDE,
    GL_UNIFORM_MATRIX_STRIDE,
    GL_UNIFORM_IS_ROW_MAJOR,
  };
  const GLint* kParams[] = {
    kBlockIndex,
    kOffset,
    kArrayStride,
    kMatrixStride,
    kIsRowMajor,
  };
  const size_t kNumIterations = std::size(kPname);
  for (size_t ii = 0; ii < kNumIterations; ++ii) {
    EXPECT_CALL(*(gl_.get()),
                GetActiveUniformsiv(
                    kServiceProgramId, data.header.num_uniforms, _,
                    kPname[ii], _))
      .WillOnce(SetArrayArgument<4>(
          kParams[ii], kParams[ii] + data.header.num_uniforms))
      .RetiresOnSaturation();
  }

  program->GetUniformsES3(&bucket);
  EXPECT_EQ(sizeof(Data), bucket.size());
  Data* bucket_data = bucket.GetDataAs<Data*>(0, sizeof(Data));
  EXPECT_TRUE(bucket_data != nullptr);
  EXPECT_EQ(0, memcmp(&data, bucket_data, sizeof(Data)));
}

// Some drivers optimize out unused uniform array elements, so their
// location would be -1.
TEST_F(ProgramManagerWithShaderTest, UnusedUniformArrayElements) {
  CommonDecoder::Bucket bucket;
  const Program* program = SetupDefaultProgram();
  ASSERT_TRUE(program != nullptr);
  // Emulate the situation that only the first element has a valid location.
  // TODO(zmo): Don't assume these are in order.
  for (size_t ii = 0; ii < std::size(kUniforms); ++ii) {
    Program::UniformInfo* uniform = const_cast<Program::UniformInfo*>(
        program->GetUniformInfo(ii));
    ASSERT_TRUE(uniform != nullptr);
    EXPECT_EQ(static_cast<size_t>(kUniforms[ii].size),
              uniform->element_locations.size());
    for (GLsizei jj = 1; jj < uniform->size; ++jj)
      uniform->element_locations[jj] = -1;
  }
  program->GetProgramInfo(manager_.get(), &bucket);
  ProgramInfoHeader* header =
      bucket.GetDataAs<ProgramInfoHeader*>(0, sizeof(ProgramInfoHeader));
  ASSERT_TRUE(header != nullptr);
  EXPECT_EQ(1u, header->link_status);
  EXPECT_EQ(std::size(kAttribs), header->num_attribs);
  EXPECT_EQ(std::size(kUniforms), header->num_uniforms);
  const ProgramInput* inputs = bucket.GetDataAs<const ProgramInput*>(
      sizeof(*header),
      sizeof(ProgramInput) * (header->num_attribs + header->num_uniforms));
  ASSERT_TRUE(inputs != nullptr);
  const ProgramInput* input = inputs + header->num_attribs;
  for (uint32_t ii = 0; ii < header->num_uniforms; ++ii) {
    const UniformInfo& expected = kUniforms[ii];
    EXPECT_EQ(expected.size, input->size);
    const int32_t* locations = bucket.GetDataAs<const int32_t*>(
        input->location_offset, sizeof(int32_t) * input->size);
    ASSERT_TRUE(locations != nullptr);
    EXPECT_EQ(
        ProgramManager::MakeFakeLocation(expected.fake_location, 0),
        locations[0]);
    for (int32_t jj = 1; jj < input->size; ++jj)
      EXPECT_EQ(-1, locations[jj]);
    ++input;
  }
}

TEST_F(ProgramManagerWithShaderTest, BindAttribLocationConflicts) {
  // Set up shader
  AttributeMap attrib_map;
  for (uint32_t ii = 0; ii < kNumAttribs; ++ii) {
    attrib_map[kAttribs[ii].name] = TestHelper::ConstructAttribute(
        kAttribs[ii].type,
        kAttribs[ii].size,
        GL_MEDIUM_FLOAT,
        kAttribStaticUse,
        kAttribs[ii].name);
  }
  const char kAttribMatName[] = "matAttrib";
  attrib_map[kAttribMatName] = TestHelper::ConstructAttribute(
      GL_FLOAT_MAT2,
      1,
      GL_MEDIUM_FLOAT,
      kAttribStaticUse,
      kAttribMatName);
  // Check we can create shader.
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  // Check shader got created.
  ASSERT_TRUE(vshader != nullptr && fshader != nullptr);
  // Set Status
  TestHelper::SetShaderStates(gl_.get(), vshader, true, nullptr, nullptr,
                              nullptr, &attrib_map, nullptr, nullptr, nullptr,
                              nullptr, nullptr);
  // Check attrib infos got copied.
  for (AttributeMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const sh::Attribute* variable_info =
        vshader->GetAttribInfo(it->first);
    ASSERT_TRUE(variable_info != nullptr);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.getOutermostArraySize(),
              variable_info->getOutermostArraySize());
    EXPECT_EQ(it->second.precision, variable_info->precision);
    EXPECT_EQ(it->second.staticUse, variable_info->staticUse);
    EXPECT_EQ(it->second.name, variable_info->name);
  }
  TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                              nullptr, &attrib_map, nullptr, nullptr, nullptr,
                              nullptr, nullptr);
  // Set up program
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));

  EXPECT_FALSE(program->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program, true));

  program->SetAttribLocationBinding(kAttrib1Name, 0);
  EXPECT_FALSE(program->DetectAttribLocationBindingConflicts());
  EXPECT_CALL(*(gl_.get()), BindAttribLocation(_, 0, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(LinkAsExpected(program, true));

  program->SetAttribLocationBinding("xxx", 0);
  EXPECT_FALSE(program->DetectAttribLocationBindingConflicts());
  EXPECT_CALL(*(gl_.get()), BindAttribLocation(_, 0, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_TRUE(LinkAsExpected(program, true));

  program->SetAttribLocationBinding(kAttrib2Name, 1);
  EXPECT_FALSE(program->DetectAttribLocationBindingConflicts());
  EXPECT_CALL(*(gl_.get()), BindAttribLocation(_, _, _))
      .Times(2)
      .RetiresOnSaturation();
  EXPECT_TRUE(LinkAsExpected(program, true));

  program->SetAttribLocationBinding(kAttrib2Name, 0);
  EXPECT_TRUE(program->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program, false));

  program->SetAttribLocationBinding(kAttribMatName, 1);
  program->SetAttribLocationBinding(kAttrib2Name, 3);
  EXPECT_CALL(*(gl_.get()), BindAttribLocation(_, _, _))
      .Times(3)
      .RetiresOnSaturation();
  EXPECT_FALSE(program->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program, true));

  program->SetAttribLocationBinding(kAttrib2Name, 2);
  EXPECT_TRUE(program->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program, false));
}

TEST_F(ProgramManagerWithShaderTest, UniformsPrecisionMismatch) {
  // Set up shader
  UniformMap vertex_uniform_map;
  vertex_uniform_map["a"] = TestHelper::ConstructUniform(
      GL_FLOAT, 3, GL_MEDIUM_FLOAT, true, "a");
  UniformMap frag_uniform_map;
  frag_uniform_map["a"] = TestHelper::ConstructUniform(
      GL_FLOAT, 3, GL_LOW_FLOAT, true, "a");

  // Check we can create shader.
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  // Check shader got created.
  ASSERT_TRUE(vshader != nullptr && fshader != nullptr);
  // Set Status
  TestHelper::SetShaderStates(gl_.get(), vshader, true, nullptr, nullptr,
                              nullptr, nullptr, &vertex_uniform_map, nullptr,
                              nullptr, nullptr, nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                              nullptr, nullptr, &frag_uniform_map, nullptr,
                              nullptr, nullptr, nullptr);
  // Set up program
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));

  std::string conflicting_name;

  EXPECT_TRUE(program->DetectUniformsMismatch(&conflicting_name));
  EXPECT_EQ("a", conflicting_name);
  EXPECT_TRUE(LinkAsExpected(program, false));
}

// If a varying has different type in the vertex and fragment
// shader, linking should fail.
TEST_F(ProgramManagerWithShaderTest, VaryingTypeMismatch) {
  const VarInfo kVertexVarying =
      { GL_FLOAT_VEC3, 1, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  const VarInfo kFragmentVarying =
      { GL_FLOAT_VEC4, 1, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  Program* program =
      SetupProgramForVariables(&kVertexVarying, 1, &kFragmentVarying, 1);

  std::string conflicting_name;

  EXPECT_TRUE(program->DetectVaryingsMismatch(&conflicting_name));
  EXPECT_EQ("a", conflicting_name);
  EXPECT_TRUE(LinkAsExpected(program, false));
}

// If a varying has different array size in the vertex and fragment
// shader, linking should fail.
TEST_F(ProgramManagerWithShaderTest, VaryingArraySizeMismatch) {
  const VarInfo kVertexVarying =
      { GL_FLOAT, 2, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  const VarInfo kFragmentVarying =
      { GL_FLOAT, 3, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  Program* program =
      SetupProgramForVariables(&kVertexVarying, 1, &kFragmentVarying, 1);

  std::string conflicting_name;

  EXPECT_TRUE(program->DetectVaryingsMismatch(&conflicting_name));
  EXPECT_EQ("a", conflicting_name);
  EXPECT_TRUE(LinkAsExpected(program, false));
}

// If a varying has different precision in the vertex and fragment
// shader, linking should succeed.
TEST_F(ProgramManagerWithShaderTest, VaryingPrecisionMismatch) {
  const VarInfo kVertexVarying =
      { GL_FLOAT, 2, GL_HIGH_FLOAT, true, "a", kVarVarying };
  const VarInfo kFragmentVarying =
      { GL_FLOAT, 2, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  Program* program =
      SetupProgramForVariables(&kVertexVarying, 1, &kFragmentVarying, 1);

  std::string conflicting_name;

  EXPECT_FALSE(program->DetectVaryingsMismatch(&conflicting_name));
  EXPECT_TRUE(conflicting_name.empty());
  EXPECT_TRUE(LinkAsExpected(program, true));
}

// If a varying is statically used in fragment shader but not
// declared in vertex shader, link should fail.
TEST_F(ProgramManagerWithShaderTest, VaryingMissing) {
  const VarInfo kFragmentVarying =
      { GL_FLOAT, 3, GL_MEDIUM_FLOAT, true, "a", kVarVarying };
  Program* program = SetupProgramForVariables(nullptr, 0, &kFragmentVarying, 1);

  std::string conflicting_name;

  EXPECT_TRUE(program->DetectVaryingsMismatch(&conflicting_name));
  EXPECT_EQ("a", conflicting_name);
  EXPECT_TRUE(LinkAsExpected(program, false));
}

// If a varying is declared but not statically used in fragment
// shader, even if it's not declared in vertex shader, link should
// succeed.
TEST_F(ProgramManagerWithShaderTest, InactiveVarying) {
  const VarInfo kFragmentVarying =
      { GL_FLOAT, 3, GL_MEDIUM_FLOAT, false, "a", kVarVarying };
  Program* program = SetupProgramForVariables(nullptr, 0, &kFragmentVarying, 1);

  std::string conflicting_name;

  EXPECT_FALSE(program->DetectVaryingsMismatch(&conflicting_name));
  EXPECT_TRUE(conflicting_name.empty());
  EXPECT_TRUE(LinkAsExpected(program, true));
}

// Uniforms and attributes are both global variables, thus sharing
// the same namespace. Any name conflicts should cause link
// failure.
TEST_F(ProgramManagerWithShaderTest, AttribUniformNameConflict) {
  const VarInfo kVertexAttribute =
      { GL_FLOAT_VEC4, 1, GL_MEDIUM_FLOAT, true, "a", kVarAttribute };
  const VarInfo kFragmentUniform =
      { GL_FLOAT_VEC4, 1, GL_MEDIUM_FLOAT, true, "a", kVarUniform };
  Program* program =
      SetupProgramForVariables(&kVertexAttribute, 1, &kFragmentUniform, 1);

  std::string conflicting_name;

  EXPECT_TRUE(program->DetectGlobalNameConflicts(&conflicting_name));
  EXPECT_EQ("a", conflicting_name);
  EXPECT_TRUE(LinkAsExpected(program, false));
}

TEST_F(ProgramManagerWithShaderTest, FragmentOutputTypes) {
  // Set up program
  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  TestHelper::SetShaderStates(gl_.get(), vshader, true, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(vshader && fshader);
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));

  {  // No outputs.
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr);
    EXPECT_TRUE(LinkAsExpected(program, true));
    EXPECT_EQ(0u, program->fragment_output_type_mask());
    EXPECT_EQ(0u, program->fragment_output_written_mask());
  }

  {  // gl_FragColor
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_FLOAT_VEC4, 0, GL_MEDIUM_FLOAT, true, "gl_FragColor");
    var.location = -1;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(LinkAsExpected(program, true));
    EXPECT_EQ(0x3u, program->fragment_output_type_mask());
    EXPECT_EQ(0x3u, program->fragment_output_written_mask());
  }

  {  // gl_FragData
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_FLOAT_VEC4, 8, GL_MEDIUM_FLOAT, true, "gl_FragData");
    var.location = -1;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(LinkAsExpected(program, true));
    EXPECT_EQ(0xFFFFu, program->fragment_output_type_mask());
    EXPECT_EQ(0xFFFFu, program->fragment_output_written_mask());
  }

  {  // gl_FragColor, gl_FragDepth
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_FLOAT_VEC4, 0, GL_MEDIUM_FLOAT, true, "gl_FragColor");
    var.location = -1;
    fragment_outputs.push_back(var);
    var = TestHelper::ConstructOutputVariable(
        GL_FLOAT, 0, GL_MEDIUM_FLOAT, true, "gl_FragDepth");
    var.location = -1;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(LinkAsExpected(program, true));
    EXPECT_EQ(0x3u, program->fragment_output_type_mask());
    EXPECT_EQ(0x3u, program->fragment_output_written_mask());
  }

  {  // Single user defined output.
    static ProgramOutputInfo kProgramOutputs[] = {{
        "myOutput",            // name
        0,                     // size
        GL_UNSIGNED_INT_VEC4,  // type
        7,                     // color_name
        0,                     // index
    }};
    const size_t kNumProgramOutputs = std::size(kProgramOutputs);
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_UNSIGNED_INT_VEC4, 0, GL_MEDIUM_INT, true, "myOutput");
    var.location = -1;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(
        LinkAsExpected(program, true, kProgramOutputs, kNumProgramOutputs));
    EXPECT_EQ(0x2u, program->fragment_output_type_mask());
    EXPECT_EQ(0x3u, program->fragment_output_written_mask());
  }

  {  // Single user defined output - no static use.
    static ProgramOutputInfo kProgramOutputs[] = {{
        "myOutput",            // name
        0,                     // size
        GL_UNSIGNED_INT_VEC4,  // type
        7,                     // color_name
        0,                     // index
    }};
    const size_t kNumProgramOutputs = std::size(kProgramOutputs);
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_UNSIGNED_INT_VEC4, 0, GL_MEDIUM_INT, false, "myOutput");
    var.location = -1;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(
        LinkAsExpected(program, true, kProgramOutputs, kNumProgramOutputs));
    EXPECT_EQ(0x2u, program->fragment_output_type_mask());
    EXPECT_EQ(0x3u, program->fragment_output_written_mask());
  }

  {  // Multiple user defined outputs.
    static ProgramOutputInfo program_outputs[] = {
        {
            "myOutput",            // name
            0,                     // size
            GL_UNSIGNED_INT_VEC4,  // type
            7,                     // color_name
            0,                     // index
        },
        {
            "myOutput1",    // name
            0,              // size
            GL_FLOAT_VEC4,  // type
            7,              // color_name
            0,              // index
        },
    };
    const size_t num_program_outputs = std::size(program_outputs);
    OutputVariableList fragment_outputs;
    sh::OutputVariable var = TestHelper::ConstructOutputVariable(
        GL_INT_VEC4, 0, GL_MEDIUM_INT, true, "myOutput");
    var.location = 0;
    fragment_outputs.push_back(var);
    var = TestHelper::ConstructOutputVariable(GL_FLOAT_VEC4, 0, GL_MEDIUM_FLOAT,
                                              true, "myOutput1");
    var.location = 2;
    fragment_outputs.push_back(var);
    TestHelper::SetShaderStates(gl_.get(), fshader, true, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                &fragment_outputs, nullptr);
    EXPECT_TRUE(
        LinkAsExpected(program, true, program_outputs, num_program_outputs));
    EXPECT_EQ(0x31u, program->fragment_output_type_mask());
    EXPECT_EQ(0x33u, program->fragment_output_written_mask());
  }
}

// Varyings go over 8 rows.
TEST_F(ProgramManagerWithShaderTest, TooManyVaryings) {
  const VarInfo kVertexVaryings[] = {
      { GL_FLOAT_VEC4, 4, GL_MEDIUM_FLOAT, true, "a", kVarVarying },
      { GL_FLOAT_VEC4, 5, GL_MEDIUM_FLOAT, true, "b", kVarVarying }
  };
  const VarInfo kFragmentVaryings[] = {
      { GL_FLOAT_VEC4, 4, GL_MEDIUM_FLOAT, true, "a", kVarVarying },
      { GL_FLOAT_VEC4, 5, GL_MEDIUM_FLOAT, true, "b", kVarVarying }
  };
  Program* program =
      SetupProgramForVariables(kVertexVaryings, 2, kFragmentVaryings, 2);

  EXPECT_FALSE(program->CheckVaryingsPacking());
  EXPECT_TRUE(LinkAsExpected(program, false));
}

// Varyings go over 8 rows but some are inactive
TEST_F(ProgramManagerWithShaderTest, TooManyInactiveVaryings) {
  const VarInfo kVertexVaryings[] = {
      { GL_FLOAT_VEC4, 4, GL_MEDIUM_FLOAT, true, "a", kVarVarying },
      { GL_FLOAT_VEC4, 5, GL_MEDIUM_FLOAT, true, "b", kVarVarying }
  };
  const VarInfo kFragmentVaryings[] = {
      { GL_FLOAT_VEC4, 4, GL_MEDIUM_FLOAT, false, "a", kVarVarying },
      { GL_FLOAT_VEC4, 5, GL_MEDIUM_FLOAT, true, "b", kVarVarying }
  };
  Program* program =
      SetupProgramForVariables(kVertexVaryings, 2, kFragmentVaryings, 2);

  EXPECT_TRUE(program->CheckVaryingsPacking());
  EXPECT_TRUE(LinkAsExpected(program, true));
}

TEST_F(ProgramManagerWithShaderTest, BindUniformLocation) {
  const GLint kUniform1DesiredLocation = 10;
  const GLint kUniform2DesiredLocation = -1;
  const GLint kUniform3DesiredLocation = 5;

  Shader* vshader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vshader, true);
  Shader* fshader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), fshader, true);
  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  EXPECT_TRUE(program->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program->SetUniformLocationBinding(
      kUniform1Name, kUniform1DesiredLocation));
  EXPECT_TRUE(program->SetUniformLocationBinding(
      kUniform3Name, kUniform3DesiredLocation));

  static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
    { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
    { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
    { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
  };
  ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2NameWithArrayIndex,
    },
    { kUniform3Name,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3NameWithArrayIndex,
    },
  };

  const size_t kNumAttribs = std::size(kAttribs);
  const size_t kNumUniforms = std::size(kUniforms);
  SetupShaderExpectations(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                          nullptr, 0, kServiceProgramId);
  program->Link(nullptr, this);

  EXPECT_EQ(kUniform1DesiredLocation,
            program->GetUniformFakeLocation(kUniform1Name));
  EXPECT_EQ(kUniform3DesiredLocation,
            program->GetUniformFakeLocation(kUniform3Name));
  EXPECT_EQ(kUniform3DesiredLocation,
            program->GetUniformFakeLocation(kUniform3NameWithArrayIndex));
}

TEST_F(ProgramManagerWithShaderTest, ZeroSizeUniformMarkedInvalid) {
  UniformInfo kInvalidUniforms[] = {
      {
          kUniform1Name, 0 /* invalid size */, kUniform1Type,
          kUniform1FakeLocation, kUniform1RealLocation,
          kUniform1DesiredLocation, kUniform1Name,
      },
  };
  const size_t kNumInvalidUniforms = std::size(kInvalidUniforms);

  SetupShaderExpectations(kAttribs, kNumAttribs, kInvalidUniforms,
                          kNumInvalidUniforms, nullptr, 0, kServiceProgramId);

  Shader* vertex_shader = shader_manager_.CreateShader(
      kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
  Shader* fragment_shader = shader_manager_.CreateShader(
      kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
  EXPECT_TRUE(vertex_shader != nullptr);
  EXPECT_TRUE(fragment_shader != nullptr);
  TestHelper::SetShaderStates(gl_.get(), vertex_shader, true);
  TestHelper::SetShaderStates(gl_.get(), fragment_shader, true);

  Program* program =
      manager_->CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program != nullptr);
  program->AttachShader(&shader_manager_, vertex_shader);
  program->AttachShader(&shader_manager_, fragment_shader);
  program->Link(nullptr, this);

  EXPECT_FALSE(program->IsValid());
}

class ProgramManagerWithCacheTest : public ProgramManagerTestBase {
 public:
  static const GLuint kClientProgramId = 1;
  static const GLuint kServiceProgramId = 10;
  static const GLuint kVertexShaderClientId = 2;
  static const GLuint kFragmentShaderClientId = 20;
  static const GLuint kVertexShaderServiceId = 3;
  static const GLuint kFragmentShaderServiceId = 30;

  ProgramManagerWithCacheTest()
      : cache_(new MockProgramCache()),
        vertex_shader_(nullptr),
        fragment_shader_(nullptr),
        program_(nullptr),
        shader_manager_(nullptr) {}

 protected:
  void SetupProgramManager() override {
    manager_ = std::make_unique<ProgramManager>(
        cache_.get(), kMaxVaryingVectors, kMaxDrawBuffers,
        kMaxDualSourceDrawBuffers, kMaxVertexAttribs, gpu_preferences_,
        feature_info_.get(), nullptr);
  }

  void SetUp() override {
    ProgramManagerTestBase::SetUp();

    vertex_shader_ = shader_manager_.CreateShader(
       kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    fragment_shader_ = shader_manager_.CreateShader(
       kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
    ASSERT_TRUE(vertex_shader_ != nullptr);
    ASSERT_TRUE(fragment_shader_ != nullptr);
    vertex_shader_->set_source("lka asjf bjajsdfj");
    fragment_shader_->set_source("lka asjf a   fasgag 3rdsf3 bjajsdfj");

    program_ = manager_->CreateProgram(kClientProgramId, kServiceProgramId);
    ASSERT_TRUE(program_ != nullptr);

    program_->AttachShader(&shader_manager_, vertex_shader_);
    program_->AttachShader(&shader_manager_, fragment_shader_);
  }

  void TearDown() override {
    vertex_shader_ = nullptr;
    fragment_shader_ = nullptr;
    shader_manager_.Destroy(false);

    program_ = nullptr;
    ProgramManagerTestBase::TearDown();
  }

  void SetShadersCompiled() {
    TestHelper::SetShaderStates(gl_.get(), vertex_shader_, true);
    TestHelper::SetShaderStates(gl_.get(), fragment_shader_, true);
  }

  void SetShadersCompiled(const std::string& compilation_options_string) {
    TestHelper::SetShaderStates(gl_.get(), vertex_shader_, true,
                                compilation_options_string);
    TestHelper::SetShaderStates(gl_.get(), fragment_shader_, true,
                                compilation_options_string);
  }

  void SetProgramCached() {
    cache_->LinkedProgramCacheSuccess(
        vertex_shader_->source(), fragment_shader_->source(),
        &program_->bind_attrib_location_map(),
        program_->effective_transform_feedback_varyings(),
        program_->effective_transform_feedback_buffer_mode());
  }

  void SetExpectationsForProgramCached() {
    SetExpectationsForProgramCached(program_,
                                    vertex_shader_,
                                    fragment_shader_);
  }

  void SetExpectationsForProgramCached(
      Program* program,
      Shader* vertex_shader,
      Shader* fragment_shader) {
    EXPECT_CALL(*cache_.get(),
                SaveLinkedProgram(
                    program->service_id(), vertex_shader, fragment_shader,
                    &program->bind_attrib_location_map(),
                    program_->effective_transform_feedback_varyings(),
                    program_->effective_transform_feedback_buffer_mode(), _))
        .Times(1);
  }

  void SetExpectationsForNotCachingProgram() {
    SetExpectationsForNotCachingProgram(program_,
                                        vertex_shader_,
                                        fragment_shader_);
  }

  void SetExpectationsForNotCachingProgram(
      Program* program,
      Shader* vertex_shader,
      Shader* fragment_shader) {
    EXPECT_CALL(*cache_.get(),
                SaveLinkedProgram(
                    program->service_id(), vertex_shader, fragment_shader,
                    &program->bind_attrib_location_map(),
                    program_->effective_transform_feedback_varyings(),
                    program_->effective_transform_feedback_buffer_mode(), _))
        .Times(0);
  }

  void SetExpectationsForProgramLoad(ProgramCache::ProgramLoadResult result) {
    SetExpectationsForProgramLoad(kServiceProgramId,
                                  program_,
                                  vertex_shader_,
                                  fragment_shader_,
                                  result);
  }

  void SetExpectationsForProgramLoad(
      GLuint service_program_id,
      Program* program,
      Shader* vertex_shader,
      Shader* fragment_shader,
      ProgramCache::ProgramLoadResult result) {
    EXPECT_CALL(*cache_.get(),
                LoadLinkedProgram(
                    service_program_id, vertex_shader, fragment_shader,
                    &program->bind_attrib_location_map(),
                    program_->effective_transform_feedback_varyings(),
                    program_->effective_transform_feedback_buffer_mode(), _))
        .WillOnce(Return(result));
  }

  void SetExpectationsForProgramLoadSuccess() {
    SetExpectationsForProgramLoadSuccess(kServiceProgramId);
  }

  void SetExpectationsForProgramLoadSuccess(GLuint service_program_id) {
    TestHelper::SetupProgramSuccessExpectations(
        gl_.get(), feature_info_.get(), nullptr, 0, nullptr, 0, nullptr, 0,
        nullptr, 0, service_program_id);
  }

  void SetExpectationsForProgramNotLoaded() {
    EXPECT_CALL(
        *cache_.get(),
        LoadLinkedProgram(
            program_->service_id(), vertex_shader_.get(),
            fragment_shader_.get(), &program_->bind_attrib_location_map(),
            program_->effective_transform_feedback_varyings(),
            program_->effective_transform_feedback_buffer_mode(), _))
        .Times(Exactly(0));
  }

  void SetExpectationsForProgramLink() {
    SetExpectationsForProgramLink(kServiceProgramId);
  }

  void SetExpectationsForProgramLink(GLuint service_program_id) {
    TestHelper::SetupShaderExpectations(gl_.get(), feature_info_.get(), nullptr,
                                        0, nullptr, 0, service_program_id);
    if (gl::g_current_gl_driver->ext.b_GL_OES_get_program_binary) {
      EXPECT_CALL(*gl_.get(),
                  ProgramParameteri(service_program_id,
                                    PROGRAM_BINARY_RETRIEVABLE_HINT,
                                    GL_TRUE)).Times(1);
    }
  }

  void SetExpectationsForSuccessCompile(
      const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source().c_str();
    EXPECT_CALL(*gl_.get(), ShaderSource(shader_id, 1, Pointee(src), nullptr))
        .Times(1);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(1);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
        .WillOnce(SetArgPointee<2>(GL_TRUE));
  }

  void SetExpectationsForNoCompile(const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source().c_str();
    EXPECT_CALL(*gl_.get(), ShaderSource(shader_id, 1, Pointee(src), nullptr))
        .Times(0);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(0);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
        .Times(0);
  }

  void SetExpectationsForErrorCompile(const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source().c_str();
    EXPECT_CALL(*gl_.get(), ShaderSource(shader_id, 1, Pointee(src), nullptr))
        .Times(1);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(1);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
        .WillOnce(SetArgPointee<2>(GL_FALSE));
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_INFO_LOG_LENGTH, _))
        .WillOnce(SetArgPointee<2>(0));
    EXPECT_CALL(*gl_.get(), GetShaderInfoLog(shader_id, 0, _, _))
        .Times(1);
  }

  std::unique_ptr<MockProgramCache> cache_;

  // These shaders are owned by |shader_manager_|.
  raw_ptr<Shader> vertex_shader_;
  raw_ptr<Shader> fragment_shader_;
  // This program is owned by |manager_|.
  raw_ptr<Program> program_;
  ShaderManager shader_manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint ProgramManagerWithCacheTest::kClientProgramId;
const GLuint ProgramManagerWithCacheTest::kServiceProgramId;
const GLuint ProgramManagerWithCacheTest::kVertexShaderClientId;
const GLuint ProgramManagerWithCacheTest::kFragmentShaderClientId;
const GLuint ProgramManagerWithCacheTest::kVertexShaderServiceId;
const GLuint ProgramManagerWithCacheTest::kFragmentShaderServiceId;
#endif

TEST_F(ProgramManagerWithCacheTest, CacheProgramOnSuccessfulLink) {
  SetShadersCompiled();
  SetExpectationsForProgramLink();
  SetExpectationsForProgramCached();
  EXPECT_TRUE(program_->Link(nullptr, this));
}

TEST_F(ProgramManagerWithCacheTest, LoadProgramOnProgramCacheHit) {
  SetShadersCompiled();
  SetProgramCached();

  SetExpectationsForNoCompile(vertex_shader_);
  SetExpectationsForNoCompile(fragment_shader_);
  SetExpectationsForProgramLoad(ProgramCache::PROGRAM_LOAD_SUCCESS);
  SetExpectationsForNotCachingProgram();
  SetExpectationsForProgramLoadSuccess();

  EXPECT_TRUE(program_->Link(nullptr, this));
}

TEST_F(ProgramManagerWithCacheTest, RelinkOnChangedCompileOptions) {
  SetShadersCompiled("a");
  SetProgramCached();
  SetExpectationsForProgramCached();

  SetShadersCompiled("b");
  SetExpectationsForProgramLink();
  SetExpectationsForProgramNotLoaded();
  EXPECT_TRUE(program_->Link(nullptr, this));
}

// For some compilers, using make_tuple("a", "bb") would end up
// instantiating make_tuple<char[1], char[2]>. This does not work.
namespace {
testing::tuple<const char*, const char*> make_gl_ext_tuple(
    const char* gl_version,
    const char* gl_extensions) {
  return testing::make_tuple(gl_version, gl_extensions);
}
}

class ProgramManagerDualSourceBlendingTest
    : public ProgramManagerWithShaderTest,
      public testing::WithParamInterface<
          testing::tuple<const char*, const char*>> {
 public:
  ProgramManagerDualSourceBlendingTest() = default;

 protected:
  void SetUpWithFeatureInfo(FeatureInfo* feature_info) {
    const char* gl_version = testing::get<0>(GetParam());
    const char* gl_extensions = testing::get<1>(GetParam());
    SetUpBase(gl_version, gl_extensions, feature_info);
  }

  void SetUp() override { SetUpWithFeatureInfo(nullptr); }
};

class ProgramManagerDualSourceBlendingES2Test
    : public ProgramManagerDualSourceBlendingTest {};

TEST_P(ProgramManagerDualSourceBlendingES2Test, UseSecondaryFragCoord) {
  DCHECK(feature_info_->feature_flags().ext_blend_func_extended);

  const VarInfo kFragmentVaryings[] = {
      {GL_FLOAT_VEC4, 0, GL_MEDIUM_FLOAT, true, "gl_SecondaryFragColorEXT",
       kVarOutput},
      {GL_FLOAT_VEC4, 0, GL_MEDIUM_FLOAT, true, "gl_FragColor", kVarOutput},
  };

  int shader_version = 100;
  Program* program =
      SetupProgramForVariables(nullptr, 0, kFragmentVaryings,
                               std::size(kFragmentVaryings), &shader_version);
  EXPECT_TRUE(LinkAsExpected(program, true));
}

TEST_P(ProgramManagerDualSourceBlendingES2Test, UseSecondaryFragData) {
  const VarInfo kFragmentVaryings[] = {
      {GL_FLOAT_VEC4, kMaxDualSourceDrawBuffers, GL_MEDIUM_FLOAT, true,
       "gl_SecondaryFragDataEXT", kVarOutput},
      {GL_FLOAT_VEC4, kMaxDrawBuffers, GL_MEDIUM_FLOAT, true, "gl_FragData",
       kVarOutput},
  };

  int shader_version = 100;
  Program* program =
      SetupProgramForVariables(nullptr, 0, kFragmentVaryings,
                               std::size(kFragmentVaryings), &shader_version);
  EXPECT_TRUE(LinkAsExpected(program, true));
}

INSTANTIATE_TEST_SUITE_P(
    SupportedContexts,
    ProgramManagerDualSourceBlendingES2Test,
    testing::Values(
        make_gl_ext_tuple("OpenGL ES 3.1",
                          "GL_EXT_draw_buffers GL_EXT_blend_func_extended")));

}  // namespace gles2
}  // namespace gpu
