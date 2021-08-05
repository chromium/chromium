// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_
#define REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capture_frame_queue.h"

namespace remoting {
namespace protocol {

// A FakeDesktopCapturer generates artificial image for testing purpose.
//
// FakeDesktopCapturer is double-buffered as required by DesktopCapturer.
class FakeDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  // By default FakeDesktopCapturer generates frames of size kWidth x kHeight,
  // but custom frame generator set using set_frame_generator() may generate
  // frames of different size.
  static const int kWidth = 800;
  static const int kHeight = 600;

  using FrameGenerator =
      base::RepeatingCallback<std::unique_ptr<webrtc::DesktopFrame>(
          webrtc::SharedMemoryFactory* shared_memory_factory)>;

  FakeDesktopCapturer();
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

  Callback* callback_;

  std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopCapturer);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_FAKE_DESKTOP_CAPTURER_H_
