// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_
#define CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_

#include "base/callback_forward.h"

class SkBitmap;

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace test_runner {

// Asks |web_frame| to print itself and calls |callback| with the result.
void PrintFrameAsync(blink::WebLocalFrame* web_frame,
                     base::OnceCallback<void(const SkBitmap&)> callback);

// Copy to clipboard the image present at |x|, |y| coordinates in |web_frame|
// and pass the captured image to |callback|.
void CopyImageAtAndCapturePixels(
    blink::WebLocalFrame* web_frame,
    int x,
    int y,
    base::OnceCallback<void(const SkBitmap&)> callback);

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_PIXEL_DUMP_H_
