// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/gpu_codec_support_waiter.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

namespace {

static constexpr base::TimeDelta kRTCGpuCodecSupportWaiterTimeout =
    base::Milliseconds(3000);

// Codec support known callback can potentially be called after the waiter is
// destroyed. RefCountedWaitableEvent is used for the event which callback sets
// to keep it alive in such case.
class RefCountedWaitableEvent
    : public base::WaitableEvent,
      public WTF::ThreadSafeRefCounted<RefCountedWaitableEvent> {
 public:
  RefCountedWaitableEvent()
      : base::WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 private:
  friend class WTF::ThreadSafeRefCounted<RefCountedWaitableEvent>;
  ~RefCountedWaitableEvent() = default;
};

void OnCodecSupportKnown(
    scoped_refptr<RefCountedWaitableEvent> codec_support_known) {
  codec_support_known->Signal();
}

}  // namespace

GpuCodecSupportWaiter::GpuCodecSupportWaiter(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories),
      wait_timeout_ms_(kRTCGpuCodecSupportWaiterTimeout) {}

bool GpuCodecSupportWaiter::IsCodecSupportKnown(bool is_encoder) const {
  if (is_encoder) {
    if (gpu_factories_->IsEncoderSupportKnown()) {
      return true;
    }
  } else if (gpu_factories_->IsDecoderSupportKnown()) {
    return true;
  }

  // crbug.com/1047994. GPU might not be initialized by the time it is queried
  // for supported codecs. Request support status notification and block
  // execution with timeout.
  // https://github.com/w3c/webrtc-extensions/issues/49 is a request for async
  // WebRTC API.

  scoped_refptr<RefCountedWaitableEvent> codec_support_known =
      base::MakeRefCounted<RefCountedWaitableEvent>();

  // Callback passed to Notify{Decoder|Decoder}SupportKnown is called on
  // caller's sequence. To not block the callback while waiting for it, request
  // notification on a separate sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  bool is_support_notification_requested = task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](media::GpuVideoAcceleratorFactories* gpu_factories,
             scoped_refptr<RefCountedWaitableEvent> codec_support_known,
             bool is_encoder) {
            if (is_encoder) {
              gpu_factories->NotifyEncoderSupportKnown(
                  base::BindOnce(&OnCodecSupportKnown, codec_support_known));
            } else {
              gpu_factories->NotifyDecoderSupportKnown(
                  base::BindOnce(&OnCodecSupportKnown, codec_support_known));
            }
          },
          gpu_factories_, codec_support_known, is_encoder));

  if (!is_support_notification_requested) {
    DLOG(WARNING) << "Failed to request codec support notification.";
    return false;
  }

  return codec_support_known->TimedWait(wait_timeout_ms_);
}

bool GpuCodecSupportWaiter::IsDecoderSupportKnown() const {
  return IsCodecSupportKnown(/*is_encoder=*/false);
}

bool GpuCodecSupportWaiter::IsEncoderSupportKnown() const {
  return IsCodecSupportKnown(/*is_encoder=*/true);
}

}  // namespace blink
