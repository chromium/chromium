// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdint.h>

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest4 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest4() = default;
};

class GLES3DecoderTest4 : public GLES2DecoderTest4 {
 public:
  GLES3DecoderTest4() { shader_language_version_ = 300; }

 protected:
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.bind_generates_resource = true;
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderTest4, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(Service, GLES3DecoderTest4, ::testing::Bool());

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_4_autogen.h"

}  // namespace gles2
}  // namespace gpu
