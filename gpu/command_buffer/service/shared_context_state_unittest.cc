// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_context_state.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_state_test_helpers.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace gpu {

class SharedContextStateTest : public ::testing::Test {
 public:
  SharedContextStateTest() = default;
};

TEST_F(SharedContextStateTest, InitFailsIfLostContext) {
  const ContextType context_type = CONTEXT_TYPE_OPENGLES2;

  // For easier substring/extension matching
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  StrictMock<gl::MockGLInterface> gl_interface;
  gl::MockGLInterface::SetGLInterface(&gl_interface);

  InSequence sequence;

  auto surface = base::MakeRefCounted<gl::GLSurfaceStub>();
  auto context = base::MakeRefCounted<gl::GLContextStub>();
  const char gl_version[] = "2.1";
  context->SetGLVersionString(gl_version);
  const char gl_extensions[] = "GL_KHR_robustness";
  context->SetExtensionsString(gl_extensions);

  context->MakeCurrent(surface.get());

  GpuFeatureInfo gpu_feature_info;
  GpuDriverBugWorkarounds workarounds;
  auto feature_info =
      base::MakeRefCounted<gles2::FeatureInfo>(workarounds, gpu_feature_info);
  gles2::TestHelper::SetupFeatureInfoInitExpectationsWithGLVersion(
      &gl_interface, gl_extensions, "", gl_version, context_type);
  feature_info->Initialize(gpu::CONTEXT_TYPE_OPENGLES2, false /* passthrough */,
                           gles2::DisallowedFeatures());

  // Setup expectations for SharedContextState::InitializeGL().
  EXPECT_CALL(gl_interface, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
      .WillOnce(SetArgPointee<1>(8u))
      .RetiresOnSaturation();
  ContextStateTestHelpers::SetupInitState(&gl_interface, feature_info.get(),
                                          gfx::Size(1, 1));

  EXPECT_CALL(gl_interface, GetGraphicsResetStatusARB())
      .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_ARB));

  auto shared_context_state = base::MakeRefCounted<SharedContextState>(
      new gl::GLShareGroup(), surface, context,
      false /* use_virtualized_gl_contexts */, base::DoNothing());

  bool result =
      shared_context_state->InitializeGL(GpuPreferences(), feature_info);
  EXPECT_FALSE(result);
}

}  // namespace gpu
