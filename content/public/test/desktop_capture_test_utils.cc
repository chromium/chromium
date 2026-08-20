// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/desktop_capture_test_utils.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/media/capture/screenshot_capture_request_impl.h"
#include "content/public/browser/desktop_capture.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content::desktop_capture {

ScopedDesktopCapturerForTesting::ScopedDesktopCapturerForTesting(
    std::unique_ptr<webrtc::DesktopCapturer> capturer) {
  SetDesktopCapturerForTesting(std::move(capturer));
}

ScopedDesktopCapturerForTesting::~ScopedDesktopCapturerForTesting() {
  SetDesktopCapturerForTesting(nullptr);
}

#if BUILDFLAG(IS_MAC)
ScopedNativePickerForTesting::ScopedNativePickerForTesting(
    Action action,
    DesktopMediaID::Id session_id,
    webrtc::DesktopCapturer::Source source) {
  SetOpenNativeScreenCapturePickerCallbackForTesting(base::BindRepeating(
      [](Action action, DesktopMediaID::Id session_id,
         webrtc::DesktopCapturer::Source source,
         content::DesktopMediaID::Type /*type*/,
         base::OnceCallback<void(content::DesktopMediaID::Id)> created_cb,
         base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_cb,
         base::OnceCallback<void()> cancel_cb,
         base::OnceCallback<void()> error_cb) {
        if (action == Action::kError) {
          std::move(error_cb).Run();
          return;
        }
        std::move(created_cb).Run(session_id);
        if (action == Action::kSelectSource) {
          std::move(picker_cb).Run(source);
        } else {
          std::move(cancel_cb).Run();
        }
      },
      action, session_id, std::move(source)));
}

ScopedNativePickerForTesting::~ScopedNativePickerForTesting() {
  SetOpenNativeScreenCapturePickerCallbackForTesting(base::NullCallback());
}
#endif

}  // namespace content::desktop_capture
