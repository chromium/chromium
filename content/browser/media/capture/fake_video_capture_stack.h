// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_

#include <memory>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {
class VideoFrame;
class VideoFrameReceiver;
}  // namespace media

namespace content {

class FakeVideoCaptureStackReceiver;

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
  bool Started() const;

  // Returns true if the device called VideoFrameReceiver::OnError().
  bool ErrorOccurred() const;

  // Accessors to capture frame queue.
  bool HasCapturedFrames() const;

  SkBitmap NextCapturedFrame();
  void ClearCapturedFramesQueue();

  // Called when tests expect there to be one or more log messages sent to the
  // video capture stack. Turn on verbose logging for a dump of the actual log
  // messages. This method clears the queue of log messages.
  void ExpectHasLogMessages();

  // Called when tests expect there to be no log messages sent to the video
  // capture stack.
  void ExpectNoLogMessages();

  using FrameReceivedCallback =
      base::RepeatingCallback<void(media::VideoFrame*)>;

  // Sets a callback that will be invoked with a pointer to VideoFrame every
  // time new frame gets added to the queue.
  void SetFrameReceivedCallback(FrameReceivedCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    on_frame_received_ = std::move(callback);
  }

  void SetStarted() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    started_ = true;
  }

  void SetErrorOccurred() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    error_occurred_ = true;
  }

  // Checks that the frame timestamp is monotonically increasing and then
  // stashes it in the |frames_| queue for later examination by the tests.
  void OnReceivedFrame(scoped_refptr<media::VideoFrame> frame);

  void OnLog(const std::string& message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    log_messages_.push_back(message);
  }

 private:
  void WaitForReceiver() const;

  bool started_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool error_occurred_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::circular_deque<std::string> log_messages_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::circular_deque<scoped_refptr<media::VideoFrame>> frames_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeDelta last_frame_timestamp_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::TimeDelta::Min();
  FrameReceivedCallback on_frame_received_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtr<FakeVideoCaptureStackReceiver> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last:
  base::WeakPtrFactory<FakeVideoCaptureStack> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_FAKE_VIDEO_CAPTURE_STACK_H_
