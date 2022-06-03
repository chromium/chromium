// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_TEST_UTILS_H_

namespace cc {
class ImageDecodeCache;
}

namespace viz {
class TestContextProvider;
}

namespace blink {

enum class SetIsContextLost { kNotModifyValue, kSetToTrue, kSetToFalse };

void InitializeSharedGpuContext(
    viz::TestContextProvider* context_provider,
    cc::ImageDecodeCache* cache = nullptr,
    SetIsContextLost set_context_lost = SetIsContextLost::kNotModifyValue);
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_GPU_TEST_UTILS_H_
