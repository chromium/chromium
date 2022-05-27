// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {
class VideoFrame;
class VideoFrameReceiver;
}  // namespace media

namespace content {

// Provides a fake representation of the entire video capture stack. It creates
// a VideoFrameReceiver that a device can deliver video frames to, and adapts
// that to a simple collector of video frames, represented as SkBitmaps, for
// further examination by the browser tests.
class FakeVideoCaptureStack {
 public:
  FakeVideoCaptureStack();
  ~FakeVideoCaptureStack();

  // Reset the capture stack to a state where it contains no frames and is
  // expecting a first frame.
  void Reset();

  // Returns a VideoFrameReceiver that the implementation under test delivers
  // frames to.
  std::unique_ptr<media::VideoFrameReceiver> CreateFrameReceiver();

  // Returns true if the device called VideoFrameReceiver::OnStarted().
  bool started() const { return started_; }

  // Returns true if the device called VideoFrameReceiver::OnError().
  bool error_occurred() const { return error_occurred_; }

  // Accessors to capture frame queue.
  bool has_captured_frames() const { return !frames_.empty(); }
  SkBitmap NextCapturedFrame();
  void ClearCapturedFramesQueue();

  // Called when tests expect there to be one or more log messages sent to the
  // video capture stack. Turn on verbose logging for a dump of the actual log
  // messages. This method clears the queue of log messages.
  void ExpectHasLogMessages();

  // Called when tests expect there to be no log messages sent to the video
  // capture stack.
  void ExpectNoLogMessages();

 private:
  // A minimal implementation of VideoFrameReceiver that wraps buffers into
  // VideoFrame instances and forwards all relevant callbacks and data to the
  // parent FakeVideoCaptureStack.
  class Receiver;

  // Checks that the frame timestamp is monotonically increasing and then
  // stashes it in the |frames_| queue for later examination by the tests.
  void OnReceivedFrame(scoped_refptr<media::VideoFrame> frame);

  bool started_ = false;
  bool error_occurred_ = false;
  base::circular_deque<std::string> log_messages_;
  base::circular_deque<scoped_refptr<media::VideoFrame>> frames_;
  base::TimeDelta last_frame_timestamp_ = base::TimeDelta::Min();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_
