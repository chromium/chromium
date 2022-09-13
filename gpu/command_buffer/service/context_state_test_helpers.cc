// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state_test_helpers.h"

#include "gpu/command_buffer/service/feature_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_version_info.h"

using ::testing::_;

namespace gpu {
// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_test_helpers_autogen.h"

void ContextStateTestHelpers::SetupInitState(MockGL* gl,
                                             gles2::FeatureInfo* feature_info,
                                             const gfx::Size& initial_size) {
  SetupInitCapabilitiesExpectations(gl, feature_info);
  SetupInitStateExpectations(gl, feature_info, initial_size);
}

void ContextStateTestHelpers::SetupInitStateManualExpectations(
    MockGL* gl,
    gles2::FeatureInfo* feature_info) {
  if (feature_info->IsES3Capable()) {
    EXPECT_CALL(*gl, PixelStorei(GL_PACK_ROW_LENGTH, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, PixelStorei(GL_UNPACK_ROW_LENGTH, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0))
        .Times(1)
        .RetiresOnSaturation();
    if (feature_info->feature_flags().ext_window_rectangles) {
      EXPECT_CALL(*gl, WindowRectanglesEXT(GL_EXCLUSIVE_EXT, 0, nullptr))
          .Times(1)
          .RetiresOnSaturation();
    }
  }
}

void ContextStateTestHelpers::SetupInitStateManualExpectationsForDoLineWidth(
    MockGL* gl,
    GLfloat width) {
  EXPECT_CALL(*gl, LineWidth(width)).Times(1).RetiresOnSaturation();
}

void ContextStateTestHelpers::ExpectEnableDisable(MockGL* gl,
                                                  GLenum cap,
                                                  bool enable) {
  if (enable) {
    EXPECT_CALL(*gl, Enable(cap)).Times(1).RetiresOnSaturation();
  } else {
    EXPECT_CALL(*gl, Disable(cap)).Times(1).RetiresOnSaturation();
  }
}

}  // namespace gpu
