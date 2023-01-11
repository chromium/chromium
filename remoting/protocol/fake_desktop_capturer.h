// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_
#define REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting::protocol {

// A FakeDesktopCapturer which generates an artificial image for testing.
//
// FakeDesktopCapturer is double-buffered as required by DesktopCapturer.
class FakeDesktopCapturer : public DesktopCapturer {
 public:
  // By default FakeDesktopCapturer generates frames of size kWidth x kHeight,
  // but custom frame generator set using set_frame_generator() may generate
  // frames of a different size.
  static const int kWidth = 800;
  static const int kHeight = 600;

  using FrameGenerator =
      base::RepeatingCallback<std::unique_ptr<webrtc::DesktopFrame>(
          webrtc::SharedMemoryFactory* shared_memory_factory)>;

  FakeDesktopCapturer();

  FakeDesktopCapturer(const FakeDesktopCapturer&) = delete;
  FakeDesktopCapturer& operator=(const FakeDesktopCapturer&) = delete;

  ~FakeDesktopCapturer() override;

  void set_frame_generator(FrameGenerator frame_generator);

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  FrameGenerator frame_generator_;

  raw_ptr<Callback> callback_;

  std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_
