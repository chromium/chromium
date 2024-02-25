// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_FRAME_SKIA_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_FRAME_SKIA_H_

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace content {

// An SkBitmap backed subclass of DesktopFrame. This enables the webrtc system
// to retain the SkBitmap buffer without having to copy the pixels out until
// they are needed (e.g., for encoding).
class DesktopFrameSkia : public webrtc::DesktopFrame {
 public:
  explicit DesktopFrameSkia(const SkBitmap& bitmap);
  DesktopFrameSkia(const DesktopFrameSkia&) = delete;
  DesktopFrameSkia& operator=(const DesktopFrameSkia&) = delete;
  ~DesktopFrameSkia() override;

 private:
  SkBitmap bitmap_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_FRAME_SKIA_H_
