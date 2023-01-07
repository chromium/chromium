// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_UTILS_H_

#include <cstdint>
#include <vector>

namespace gfx {
class Size;
}  // namespace gfx

namespace gpu {
class SharedContextState;
class SharedImageRepresentationFactory;
struct Mailbox;

std::vector<uint8_t> ReadPixels(
    Mailbox mailbox,
    gfx::Size size,
    SharedContextState* context_state,
    SharedImageRepresentationFactory* representation_factory);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_TEST_UTILS_H_
