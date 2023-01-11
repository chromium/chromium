// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_H_
#define REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_H_

#include "base/functional/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"

namespace remoting {

// Wrapper around webrtc::DesktopAndCursorComposer to execute methods that
// return a value in an async manner on capturer thread.
class DesktopAndCursorComposer : public webrtc::DesktopAndCursorComposer {
 public:
#if defined(WEBRTC_USE_GIO)
  virtual void GetMetadataAsync(
      base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback);
#endif
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_AND_CURSOR_COMPOSER_H_
