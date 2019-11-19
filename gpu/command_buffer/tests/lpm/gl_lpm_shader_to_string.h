// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_LPM_GL_LPM_SHADER_TO_STRING_H_
#define GPU_COMMAND_BUFFER_TESTS_LPM_GL_LPM_SHADER_TO_STRING_H_

#include <string>

#include "gpu/command_buffer/tests/lpm/gl_lpm_fuzzer.pb.h"

namespace gl_lpm_fuzzer {

std::string GetShader(const fuzzing::Shader& shader);

}  // namespace gl_lpm_fuzzer

#endif  // GPU_COMMAND_BUFFER_TESTS_LPM_GL_LPM_SHADER_TO_STRING_H_
