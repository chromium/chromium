// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INPUT_UTILS_H_
#define PDF_INPUT_UTILS_H_

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace chrome_pdf {

// Normalize a blink::WebMouseEvent. For macOS, normalization means transforming
// the ctrl + left button down events into a right button down event.
blink::WebMouseEvent NormalizeMouseEvent(const blink::WebMouseEvent& event);

}  // namespace chrome_pdf

#endif  // PDF_INPUT_UTILS_H_
