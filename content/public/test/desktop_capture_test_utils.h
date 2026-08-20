// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DESKTOP_CAPTURE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_DESKTOP_CAPTURE_TEST_UTILS_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace webrtc {
class DesktopCapturer;
}

namespace content::desktop_capture {

// Scoped desktop capturer override for testing to prevent leaks across tests.
class ScopedDesktopCapturerForTesting {
 public:
  explicit ScopedDesktopCapturerForTesting(
      std::unique_ptr<webrtc::DesktopCapturer> capturer);
  ~ScopedDesktopCapturerForTesting();

  ScopedDesktopCapturerForTesting(const ScopedDesktopCapturerForTesting&) =
      delete;
  ScopedDesktopCapturerForTesting& operator=(
      const ScopedDesktopCapturerForTesting&) = delete;
};

#if BUILDFLAG(IS_MAC)
// Scoped native screen capture picker override for testing.
class ScopedNativePickerForTesting {
 public:
  enum class Action { kSelectSource, kCancel, kError };

  explicit ScopedNativePickerForTesting(
      Action action,
      DesktopMediaID::Id session_id = 1,
      webrtc::DesktopCapturer::Source source = {42, "Mock Native Window"});
  ~ScopedNativePickerForTesting();

  ScopedNativePickerForTesting(const ScopedNativePickerForTesting&) = delete;
  ScopedNativePickerForTesting& operator=(const ScopedNativePickerForTesting&) =
      delete;
};
#endif

}  // namespace content::desktop_capture

#endif  // CONTENT_PUBLIC_TEST_DESKTOP_CAPTURE_TEST_UTILS_H_
