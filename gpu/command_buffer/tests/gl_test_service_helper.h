// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_
#define GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_

#include <memory>

namespace gpu {
class GLManager;
struct Mailbox;
class MemoryTypeTracker;
class SharedImageRepresentationFactoryRef;

bool InspectTextureLevelSize(GLManager* gl_manager,
                             unsigned int client_id,
                             unsigned int target,
                             int level,
                             int* width,
                             int* height);

// Creates and registers an uncleared RGBA_F16 TestImageBacking backed by a real
// GL texture whose underlying memory is pre-populated with bright red
// (1.0, 0.0, 0.0, 1.0) in half-float format, simulating dirty uninitialized
// GPU memory.
std::unique_ptr<SharedImageRepresentationFactoryRef>
RegisterDirtyUnclearedHalfFloatSharedImage(GLManager* gl_manager,
                                           MemoryTypeTracker* memory_tracker,
                                           const Mailbox& mailbox,
                                           int width,
                                           int height);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_GL_TEST_SERVICE_HELPER_H_
