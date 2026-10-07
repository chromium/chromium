// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_SCREEN_CAPTURE_PICKER_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_SCREEN_CAPTURE_PICKER_MAC_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/common/content_export.h"

namespace content {

// FakeScreenCapturePickerMac is used for testing and automated environments
// where a real system picker UI is not desirable.
//
// When active, it automatically selects a capture source based on the
// requested DesktopMediaID::Type:
// - For TYPE_SCREEN, it captures the display with the largest overlap with
//   the selected browser window.
// - For TYPE_WINDOW, it captures the selected browser window.
//
// The "selected browser window" is the largest window owned by the current
// browser process. If `browser_window_title` is provided, only windows
// with a title containing that string are considered.
//
// Note: This picker relies on the fact that SCShareableContent reveals the
// current process's own windows even without explicit system screen recording
// permissions. Therefore, it can *only* capture the browser's own windows
// and displays containing them.
CONTENT_EXPORT std::unique_ptr<NativeScreenCapturePicker>
CreateFakeScreenCapturePickerMac(
    std::optional<std::string> browser_window_title,
    base::OnceClosure stop_callback = base::DoNothing());

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_SCREEN_CAPTURE_PICKER_MAC_H_
