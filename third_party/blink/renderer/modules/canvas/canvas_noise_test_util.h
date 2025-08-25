// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS_NOISE_TEST_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS_NOISE_TEST_UTIL_H_

#include <memory>

namespace viz {
class TestRasterInterface;
}

namespace blink {

// Creates a raster interface that always returns the same randomized image when
// read back.
std::unique_ptr<viz::TestRasterInterface>
CreateCanvasNoiseTestRasterInterface();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS_NOISE_TEST_UTIL_H_
