// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_SKIA_BITMAP_DESKTOP_FRAME_H_
#define REMOTING_HOST_CHROMEOS_SKIA_BITMAP_DESKTOP_FRAME_H_

#include <stdint.h>

#include <memory>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// DesktopFrame implementation used by screen capture on ChromeOS.
// Frame data is stored in a SkBitmap.
class SkiaBitmapDesktopFrame : public webrtc::DesktopFrame {
 public:
  static SkiaBitmapDesktopFrame* Create(std::unique_ptr<SkBitmap> bitmap);

  SkiaBitmapDesktopFrame(const SkiaBitmapDesktopFrame&) = delete;
  SkiaBitmapDesktopFrame& operator=(const SkiaBitmapDesktopFrame&) = delete;

  ~SkiaBitmapDesktopFrame() override;

 private:
  SkiaBitmapDesktopFrame(webrtc::DesktopSize size,
                         int stride,
                         uint8_t* data,
                         std::unique_ptr<SkBitmap> bitmap);

  std::unique_ptr<SkBitmap> bitmap_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_SKIA_BITMAP_DESKTOP_FRAME_H_
