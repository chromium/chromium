// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
#define REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

// Interface for capturing desktop frames. Note that unlike
// `webrtc::DesktopCapturer`, `remoting::DesktopCapturer` schedules frame
// capturing internally (or receives push frames) and delivers frames whenever
// they are available.
class DesktopCapturer {
 public:
  using Callback = webrtc::DesktopCapturer::Callback;
  using SourceId = webrtc::DesktopCapturer::SourceId;

  virtual ~DesktopCapturer() = default;

  virtual void Start(Callback* callback) = 0;

  virtual void SetSharedMemoryFactory(
      std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

  virtual void SelectSource(SourceId id) {}

  // TODO: crbug.com/375470501 - Remove this method once FakeDesktopCapturer
  // schedules its own frames.
  virtual void CaptureFrame() {}

  virtual void SetMaxFrameRate(uint32_t max_frame_rate) {}

  // Pauses or unpauses the capturer.
  virtual void Pause(bool pause) {}

  // Temporarily adjusts the capture rate to `capture_interval` for the next
  // `duration`.
  virtual void BoostCaptureRate(base::TimeDelta capture_interval,
                                base::TimeDelta duration) {}
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
