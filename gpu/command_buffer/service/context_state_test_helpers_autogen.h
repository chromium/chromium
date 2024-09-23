// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by context_state_test_helpers.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_TEST_HELPERS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_TEST_HELPERS_AUTOGEN_H_

void ContextStateTestHelpers::SetupInitCapabilitiesExpectations(
    MockGL* gl,
    gles2::FeatureInfo* feature_info) {
  ExpectEnableDisable(gl, GL_BLEND, false);
  ExpectEnableDisable(gl, GL_CULL_FACE, false);
  ExpectEnableDisable(gl, GL_DEPTH_TEST, false);
  ExpectEnableDisable(gl, GL_DITHER, true);
  ExpectEnableDisable(gl, GL_POLYGON_OFFSET_FILL, false);
  ExpectEnableDisable(gl, GL_SAMPLE_ALPHA_TO_COVERAGE, false);
  ExpectEnableDisable(gl, GL_SAMPLE_COVERAGE, false);
  ExpectEnableDisable(gl, GL_SCISSOR_TEST, false);
  ExpectEnableDisable(gl, GL_STENCIL_TEST, false);
  if (feature_info->feature_flags().ext_multisample_compatibility) {
    ExpectEnableDisable(gl, GL_MULTISAMPLE_EXT, true);
  }
  if (feature_info->feature_flags().ext_multisample_compatibility) {
    ExpectEnableDisable(gl, GL_SAMPLE_ALPHA_TO_ONE_EXT, false);
  }
  if (feature_info->IsES3Capable()) {
    ExpectEnableDisable(gl, GL_RASTERIZER_DISCARD, false);
    ExpectEnableDisable(gl, GL_PRIMITIVE_RESTART_FIXED_INDEX, false);
  }
}

void ContextStateTestHelpers::SetupInitStateExpectations(
    MockGL* gl,
    gles2::FeatureInfo* feature_info,
    const gfx::Size& initial_size) {
  EXPECT_CALL(*gl, BlendColor(0.0f, 0.0f, 0.0f, 0.0f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, ClearColor(0.0f, 0.0f, 0.0f, 0.0f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, ClearDepth(1.0f)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, ClearStencil(0)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, ColorMask(true, true, true, true))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, CullFace(GL_BACK)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, DepthFunc(GL_LESS)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, DepthMask(true)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, DepthRange(0.0f, 1.0f)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, FrontFace(GL_CCW)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, Hint(GL_GENERATE_MIPMAP_HINT, GL_DONT_CARE))
      .Times(1)
      .RetiresOnSaturation();
  if (feature_info->feature_flags().oes_standard_derivatives) {
    EXPECT_CALL(*gl, Hint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES, GL_DONT_CARE))
        .Times(1)
        .RetiresOnSaturation();
  }
  SetupInitStateManualExpectationsForDoLineWidth(gl, 1.0f);
  EXPECT_CALL(*gl, PixelStorei(GL_PACK_ALIGNMENT, 4))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, PixelStorei(GL_UNPACK_ALIGNMENT, 4))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, PolygonOffset(0.0f, 0.0f)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, SampleCoverage(1.0f, false)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl, Scissor(0, 0, initial_size.width(), initial_size.height()))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xFFFFFFFFU))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFFFFFFFFU))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilMaskSeparate(GL_FRONT, 0xFFFFFFFFU))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilMaskSeparate(GL_BACK, 0xFFFFFFFFU))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, StencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, Viewport(0, 0, initial_size.width(), initial_size.height()))
      .Times(1)
      .RetiresOnSaturation();
  SetupInitStateManualExpectations(gl, feature_info);
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_CONTEXT_STATE_TEST_HELPERS_AUTOGEN_H_
