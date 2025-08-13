// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_

#include <cstdint>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// DesktopCapturer implementation that allows capturing a single screen via the
// provided PipewireCaptureStream.
class PipewireDesktopCapturer : public DesktopCapturer {
 public:
  PipewireDesktopCapturer();
  PipewireDesktopCapturer(const PipewireDesktopCapturer&) = delete;
  PipewireDesktopCapturer& operator=(const PipewireDesktopCapturer&) = delete;
  ~PipewireDesktopCapturer() override;

  // Returns a callback to initialize PipewireDesktopCapturer. The callback must
  // be called exactly once on the sequence where this object was constructed.
  // The `base::WeakPtr<PipewireCaptureStream>` also must be (or will be) bound
  // to the creating sequence. Calls of DesktopCapturer APIs will be handled
  // after the callback is called.
  base::OnceCallback<void(base::WeakPtr<PipewireCaptureStream>)>
  GetInitCallback();

  // DesktopCapturer interface. Methods will be called on the capture sequence.
  bool SupportsFrameCallbacks() override;
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  void SetMaxFrameRate(std::uint32_t max_frame_rate) override;

  // Unimplemented DesktopCapturer methods that should not be called. Rather,
  // the appropriate PipewireCaptureStream is provided to the constructor by the
  // DesktopInteractionStrategy based on the screen ID passed to
  // DesktopInteractionStrategy::CreateVideoCapturer().
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  class Core;

  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PIPEWIRE_DESKTOP_CAPTURER_H_
