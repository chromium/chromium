// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_

namespace gpu {
class GLManager;

bool InspectTextureLevelSize(GLManager* gl_manager,
                             unsigned int client_id,
                             unsigned int target,
                             int level,
                             int* width,
                             int* height);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_
