// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/desktop_capture_test_utils.h"

#include <utility>

#include "base/check.h"
#include "content/browser/media/capture/screenshot_capture_request_impl.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content::desktop_capture {

ScopedDesktopCapturerForTesting::ScopedDesktopCapturerForTesting(
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {
  SetDesktopCapturerForTesting(std::move(capturer));
}

ScopedDesktopCapturerForTesting::~ScopedDesktopCapturerForTesting() {
  SetDesktopCapturerForTesting(nullptr);
}

}  // namespace content::desktop_capture
