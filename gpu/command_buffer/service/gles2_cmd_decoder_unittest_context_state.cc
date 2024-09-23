// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

namespace {

void SetupUpdateES3UnpackParametersExpectations(::gl::MockGLInterface* gl,
                                                GLint row_length,
                                                GLint image_height) {
  EXPECT_CALL(*gl, PixelStorei(GL_UNPACK_ROW_LENGTH, row_length))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, image_height))
      .Times(1)
      .RetiresOnSaturation();
}

}  // namespace anonymous

class GLES2DecoderRestoreStateTest : public GLES2DecoderManualInitTest {
 public:
  GLES2DecoderRestoreStateTest() = default;

 protected:
  void AddExpectationsForActiveTexture(GLenum unit);
  void AddExpectationsForBindTexture(GLenum target, GLuint id);
  void InitializeContextState(ContextState* state,
                              uint32_t non_default_unit,
                              uint32_t active_unit);

  // ES3 specific.
  scoped_refptr<FeatureInfo> SetupForES3Test();
  void AddExpectationsForBindSampler(GLuint unit, GLuint id);
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderRestoreStateTest,
                         ::testing::Bool());

void GLES2DecoderRestoreStateTest::AddExpectationsForActiveTexture(
    GLenum unit) {
  EXPECT_CALL(*gl_, ActiveTexture(unit)).Times(1).RetiresOnSaturation();
}

void GLES2DecoderRestoreStateTest::AddExpectationsForBindTexture(GLenum target,
                                                                 GLuint id) {
  EXPECT_CALL(*gl_, BindTexture(target, id)).Times(1).RetiresOnSaturation();
}

void GLES2DecoderRestoreStateTest::InitializeContextState(
    ContextState* state,
    uint32_t non_default_unit,
    uint32_t active_unit) {
  state->texture_units.resize(group().max_texture_units());
  for (uint32_t tt = 0; tt < state->texture_units.size(); ++tt) {
    TextureRef* ref_cube_map =
        group().texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_CUBE_MAP);
    state->texture_units[tt].bound_texture_cube_map = ref_cube_map;
    TextureRef* ref_2d =
        (tt == non_default_unit)
            ? group().texture_manager()->GetTexture(client_texture_id_)
            : group().texture_manager()->GetDefaultTextureInfo(GL_TEXTURE_2D);
    state->texture_units[tt].bound_texture_2d = ref_2d;
  }
  state->active_texture_unit = active_unit;

  // Set up the sampler units just for convenience of the ES3-specific
  // tests in this file.
  state->sampler_units.resize(group().max_texture_units());
}

scoped_refptr<FeatureInfo> GLES2DecoderRestoreStateTest::SetupForES3Test() {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);

  // Construct a previous ContextState assuming an ES3 context and with all
  // texture bindings set to default textures.
  scoped_refptr<FeatureInfo> feature_info = new FeatureInfo;
  TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
      gl_.get(), "", "", "OpenGL ES 3.0", CONTEXT_TYPE_OPENGLES3);
  feature_info->InitializeForTesting(CONTEXT_TYPE_OPENGLES3);
  return feature_info;
}

void GLES2DecoderRestoreStateTest::AddExpectationsForBindSampler(GLuint unit,
                                                                 GLuint id) {
  EXPECT_CALL(*gl_, BindSampler(unit, id)).Times(1).RetiresOnSaturation();
}

TEST_P(GLES2DecoderRestoreStateTest, NullPreviousStateBGR) {
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  SetupTexture();

  InSequence sequence;
  // Expect to restore texture bindings for unit GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);
  AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP,
                                TestHelper::kServiceDefaultTextureCubemapId);

  // Expect to restore texture bindings for remaining units.
  for (uint32_t i = 1; i < group().max_texture_units(); ++i) {
    AddExpectationsForActiveTexture(GL_TEXTURE0 + i);
    AddExpectationsForBindTexture(GL_TEXTURE_2D,
                                  TestHelper::kServiceDefaultTexture2dId);
    AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP,
                                  TestHelper::kServiceDefaultTextureCubemapId);
  }

  // Expect to restore the active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(nullptr);
}

TEST_P(GLES2DecoderRestoreStateTest, NullPreviousState) {
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  InitDecoder(init);
  SetupTexture();

  InSequence sequence;
  // Expect to restore texture bindings for unit GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);
  AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP, 0);

  // Expect to restore texture bindings for remaining units.
  for (uint32_t i = 1; i < group().max_texture_units(); ++i) {
    AddExpectationsForActiveTexture(GL_TEXTURE0 + i);
    AddExpectationsForBindTexture(GL_TEXTURE_2D, 0);
    AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  }

  // Expect to restore the active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(nullptr);
}

TEST_P(GLES2DecoderRestoreStateTest, WithPreviousStateBGR) {
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  SetupTexture();

  // Construct a previous ContextState with all texture bindings
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, std::numeric_limits<uint32_t>::max(), 0);

  InSequence sequence;
  // Expect to restore only GL_TEXTURE_2D binding for GL_TEXTURE0 unit,
  // since the rest of the bindings haven't changed between the current
  // state and the |prev_state|.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, WithPreviousState) {
  InitState init;
  InitDecoder(init);
  SetupTexture();

  // Construct a previous ContextState with all texture bindings
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, std::numeric_limits<uint32_t>::max(), 0);

  InSequence sequence;
  // Expect to restore only GL_TEXTURE_2D binding for GL_TEXTURE0 unit,
  // since the rest of the bindings haven't changed between the current
  // state and the |prev_state|.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, ActiveUnit1) {
  InitState init;
  InitDecoder(init);

  // Bind a non-default texture to GL_TEXTURE1 unit.
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  cmds::ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  SetupTexture();

  // Construct a previous ContextState with all texture bindings
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, std::numeric_limits<uint32_t>::max(), 0);

  InSequence sequence;
  // Expect to restore only GL_TEXTURE_2D binding for GL_TEXTURE1 unit,
  // since the rest of the bindings haven't changed between the current
  // state and the |prev_state|.
  AddExpectationsForActiveTexture(GL_TEXTURE1);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore active texture unit to GL_TEXTURE1.
  AddExpectationsForActiveTexture(GL_TEXTURE1);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, NonDefaultUnit0BGR) {
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);

  // Bind a non-default texture to GL_TEXTURE1 unit.
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  SpecializedSetup<cmds::ActiveTexture, 0>(true);
  cmds::ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  SetupTexture();

  // Construct a previous ContextState with GL_TEXTURE_2D target in
  // GL_TEXTURE0 unit bound to a non-default texture and the rest
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, 0, kServiceTextureId);

  InSequence sequence;
  // Expect to restore GL_TEXTURE_2D binding for GL_TEXTURE0 unit to
  // a default texture.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D,
                                TestHelper::kServiceDefaultTexture2dId);

  // Expect to restore GL_TEXTURE_2D binding for GL_TEXTURE1 unit to
  // non-default.
  AddExpectationsForActiveTexture(GL_TEXTURE1);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore active texture unit to GL_TEXTURE1.
  AddExpectationsForActiveTexture(GL_TEXTURE1);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, NonDefaultUnit1BGR) {
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);

  // Bind a non-default texture to GL_TEXTURE0 unit.
  SetupTexture();

  // Construct a previous ContextState with GL_TEXTURE_2D target in
  // GL_TEXTURE1 unit bound to a non-default texture and the rest
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, 1, kServiceTextureId);

  InSequence sequence;
  // Expect to restore GL_TEXTURE_2D binding to the non-default texture
  // for GL_TEXTURE0 unit.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore GL_TEXTURE_2D binding to the default texture
  // for GL_TEXTURE1 unit.
  AddExpectationsForActiveTexture(GL_TEXTURE1);
  AddExpectationsForBindTexture(GL_TEXTURE_2D,
                                TestHelper::kServiceDefaultTexture2dId);

  // Expect to restore active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, DefaultUnit0) {
  InitState init;
  InitDecoder(init);

  // Bind a non-default texture to GL_TEXTURE1 unit.
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  SpecializedSetup<cmds::ActiveTexture, 0>(true);
  cmds::ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  SetupTexture();

  // Construct a previous ContextState with GL_TEXTURE_2D target in
  // GL_TEXTURE0 unit bound to a non-default texture and the rest
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, 0, kServiceTextureId);

  InSequence sequence;
  // Expect to restore GL_TEXTURE_2D binding for GL_TEXTURE0 unit to
  // the 0 texture.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, 0);

  // Expect to restore GL_TEXTURE_2D binding for GL_TEXTURE1 unit to
  // non-default.
  AddExpectationsForActiveTexture(GL_TEXTURE1);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore active texture unit to GL_TEXTURE1.
  AddExpectationsForActiveTexture(GL_TEXTURE1);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, DefaultUnit1) {
  InitState init;
  InitDecoder(init);

  // Bind a non-default texture to GL_TEXTURE0 unit.
  SetupTexture();

  // Construct a previous ContextState with GL_TEXTURE_2D target in
  // GL_TEXTURE1 unit bound to a non-default texture and the rest
  // set to default textures.
  ContextState prev_state(nullptr);
  InitializeContextState(&prev_state, 1, kServiceTextureId);

  InSequence sequence;
  // Expect to restore GL_TEXTURE_2D binding to the non-default texture
  // for GL_TEXTURE0 unit.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);

  // Expect to restore GL_TEXTURE_2D binding to the 0 texture
  // for GL_TEXTURE1 unit.
  AddExpectationsForActiveTexture(GL_TEXTURE1);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, 0);

  // Expect to restore active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, ES3NullPreviousStateWithSampler) {
  // This ES3-specific test is scoped within GLES2DecoderRestoreStateTest
  // to avoid doing large refactorings of these tests.
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);
  SetupTexture();
  SetupSampler();

  InSequence sequence;
  // Expect to restore texture bindings for unit GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D, kServiceTextureId);
  AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  AddExpectationsForBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  AddExpectationsForBindTexture(GL_TEXTURE_3D, 0);
  // Expect to restore sampler binding for unit GL_TEXTURE0.
  AddExpectationsForBindSampler(0, kServiceSamplerId);

  // Expect to restore texture bindings for remaining units.
  for (uint32_t i = 1; i < group().max_texture_units(); ++i) {
    AddExpectationsForActiveTexture(GL_TEXTURE0 + i);
    AddExpectationsForBindTexture(GL_TEXTURE_2D, 0);
    AddExpectationsForBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    AddExpectationsForBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    AddExpectationsForBindTexture(GL_TEXTURE_3D, 0);
    AddExpectationsForBindSampler(i, 0);
  }

  // Expect to restore the active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(nullptr);
}

TEST_P(GLES2DecoderRestoreStateTest, ES3RestoreExistingSampler) {
  // This ES3-specific test is scoped within GLES2DecoderRestoreStateTest
  // to avoid doing large refactorings of these tests.
  auto feature_info = SetupForES3Test();
  SetupSampler();

  // Construct a previous ContextState assuming an ES3 context and with all
  // texture bindings set to default textures.
  ContextState prev_state(feature_info.get());
  InitializeContextState(&prev_state, std::numeric_limits<uint32_t>::max(), 0);

  InSequence sequence;
  // Expect to restore sampler binding for unit GL_TEXTURE0.
  AddExpectationsForBindSampler(0, kServiceSamplerId);

  // Expect to restore the active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);
}

TEST_P(GLES2DecoderRestoreStateTest, ES3RestoreZeroSampler) {
  // This ES3-specific test is scoped within GLES2DecoderRestoreStateTest
  // to avoid doing large refactorings of these tests.
  auto feature_info = SetupForES3Test();

  // Construct a previous ContextState assuming an ES3 context and with all
  // texture bindings set to default textures.
  SamplerManager sampler_manager(feature_info.get());
  ContextState prev_state(feature_info.get());
  InitializeContextState(&prev_state, std::numeric_limits<uint32_t>::max(), 0);
  // Set up a sampler in the previous state. The client_id and service_id
  // don't matter except that they're non-zero.
  prev_state.sampler_units[0] = new Sampler(&sampler_manager, 1, 2);

  InSequence sequence;
  // Expect to restore the zero sampler on unit GL_TEXTURE0.
  AddExpectationsForBindSampler(0, 0);

  // Expect to restore the active texture unit to GL_TEXTURE0.
  AddExpectationsForActiveTexture(GL_TEXTURE0);

  GetDecoder()->RestoreAllTextureUnitAndSamplerBindings(&prev_state);

  // Tell the sampler manager to destroy itself without a context so we
  // don't have to set up more expectations.
  sampler_manager.Destroy(false);
}

TEST_P(GLES2DecoderManualInitTest, ContextStateCapabilityCaching) {
  struct TestInfo {
    GLenum gl_enum;
    bool default_state;
    bool expect_set;
  };

  // TODO(vmiura): Should autogen this to match build_gles2_cmd_buffer.py.
  TestInfo test[] = {{GL_BLEND, false, true},
                     {GL_CULL_FACE, false, true},
                     {GL_DEPTH_TEST, false, false},
                     {GL_DITHER, true, true},
                     {GL_POLYGON_OFFSET_FILL, false, true},
                     {GL_SAMPLE_ALPHA_TO_COVERAGE, false, true},
                     {GL_SAMPLE_COVERAGE, false, true},
                     {GL_SCISSOR_TEST, false, true},
                     {GL_STENCIL_TEST, false, false},
                     {0, false, false}};

  InitState init;
  InitDecoder(init);

  for (int i = 0; test[i].gl_enum; i++) {
    bool enable_state = test[i].default_state;

    // Test setting default state initially is ignored.
    EnableDisableTest(test[i].gl_enum, enable_state, test[i].expect_set);

    // Test new and cached state changes.
    for (int n = 0; n < 3; n++) {
      enable_state = !enable_state;
      EnableDisableTest(test[i].gl_enum, enable_state, test[i].expect_set);
      EnableDisableTest(test[i].gl_enum, enable_state, test[i].expect_set);
    }
  }
}

TEST_P(GLES3DecoderTest, ES3PixelStoreiWithPixelUnpackBuffer) {
  // Without PIXEL_UNPACK_BUFFER bound, PixelStorei with unpack parameters
  // is cached and not passed down to GL.
  EXPECT_CALL(*gl_, PixelStorei(_, _)).Times(0);
  cmds::PixelStorei cmd;
  cmd.Init(GL_UNPACK_ROW_LENGTH, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // When a PIXEL_UNPACK_BUFFER is bound, all cached unpack parameters are
  // applied to GL.
  SetupUpdateES3UnpackParametersExpectations(gl_.get(), 8, 0);
  DoBindBuffer(GL_PIXEL_UNPACK_BUFFER, client_buffer_id_, kServiceBufferId);

  // Now with a bound PIXEL_UNPACK_BUFFER, all PixelStorei calls with unpack
  // parameters are applied to GL.
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, 16))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(GL_UNPACK_ROW_LENGTH, 16);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // Now unbind PIXEL_UNPACK_BUFFER, all ES3 unpack parameters are set back to
  // 0.
  SetupUpdateES3UnpackParametersExpectations(gl_.get(), 0, 0);
  DoBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0, 0);

  // Again, PixelStorei calls with unpack parameters are cached.
  EXPECT_CALL(*gl_, PixelStorei(_, _)).Times(0);
  cmd.Init(GL_UNPACK_ROW_LENGTH, 32);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // Bind a PIXEL_UNPACK_BUFFER again.
  SetupUpdateES3UnpackParametersExpectations(gl_.get(), 32, 0);
  DoBindBuffer(GL_PIXEL_UNPACK_BUFFER, client_buffer_id_, kServiceBufferId);
}

// TODO(vmiura): Tests for VAO restore.

// TODO(vmiura): Tests for ContextState::RestoreAttribute().

// TODO(vmiura): Tests for ContextState::RestoreBufferBindings().

// TODO(vmiura): Tests for ContextState::RestoreProgramBindings().

// TODO(vmiura): Tests for ContextState::RestoreRenderbufferBindings().

// TODO(vmiura): Tests for ContextState::RestoreProgramBindings().

// TODO(vmiura): Tests for ContextState::RestoreGlobalState().

}  // namespace gles2
}  // namespace gpu
