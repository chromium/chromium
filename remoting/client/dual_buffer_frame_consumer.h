// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DUAL_BUFFER_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_DUAL_BUFFER_FRAME_CONSUMER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

// This class continuously uses two BasicDesktopFrame as buffer for decoding
// updated regions until the resolution is changed.
// This class should be used and destroyed on the same thread. If |task_runner|
// is null |callback| will be run directly upon the stack of DrawFrame,
// otherwise a task will be posted to feed the callback on the thread of
// |task_runner|.
// Only areas bound by updated_region() on the buffer are considered valid to
// |callback|. Please use RequestFullDesktopFrame() if you want to get a full
// desktop frame.
class DualBufferFrameConsumer : public protocol::FrameConsumer {
 public:
  // RenderCallback(decoded_frame, done)
  // |done| should be run after it is rendered. Can be called on any thread.
  using RenderCallback =
      base::Callback<void(std::unique_ptr<webrtc::DesktopFrame>,
                          const base::Closure&)>;
  DualBufferFrameConsumer(
      const RenderCallback& callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      PixelFormat format);
  ~DualBufferFrameConsumer() override;

  // Feeds the callback on the right thread with a BasicDesktopFrame that merges
  // updates from buffer_[0] and buffer_[1]. Do nothing if no updates have
  // received yet.
  void RequestFullDesktopFrame();

  // FrameConsumer interface.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done) override;
  PixelFormat GetPixelFormat() override;

  base::WeakPtr<DualBufferFrameConsumer> GetWeakPtr();

 private:
  void RunRenderCallback(std::unique_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done);

  std::unique_ptr<webrtc::SharedDesktopFrame> buffers_[2];

  // Represents dirty regions that are currently in buffers_[1]. Will be used
  // when calling RequestFullDesktopFrame() to construct the full desktop frame.
  webrtc::DesktopRegion buffer_1_mask_;

  int current_buffer_ = 0;

  RenderCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  PixelFormat pixel_format_;
  base::ThreadChecker thread_checker_;
  base::WeakPtr<DualBufferFrameConsumer> weak_ptr_;
  base::WeakPtrFactory<DualBufferFrameConsumer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DualBufferFrameConsumer);
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_DUAL_BUFFER_FRAME_CONSUMER_H_
