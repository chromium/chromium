// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

using webrtc::Metronome;

// This delegate class exists to work around the fact that
// RTCEncodedVideoStreamTransformer cannot derive from rtc::RefCountedObject
// and post tasks referencing itself as an rtc::scoped_refptr. Instead,
// RTCEncodedVideoStreamTransformer creates a delegate using
// rtc::RefCountedObject and posts tasks referencing the delegate, which
// invokes the RTCEncodedVideoStreamTransformer via callbacks.
class RTCEncodedVideoStreamTransformerDelegate
    : public webrtc::FrameTransformerInterface {
 public:
  RTCEncodedVideoStreamTransformerDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner,
      scoped_refptr<RTCEncodedVideoStreamTransformer::Broker>
          transformer_broker,
      std::unique_ptr<Metronome> metronome)
      : source_task_runner_(realm_task_runner),
        transformer_broker_(std::move(transformer_broker)),
        metronome_(std::move(metronome)) {
    DCHECK(source_task_runner_->BelongsToCurrentThread());
    DETACH_FROM_SEQUENCE(metronome_sequence_checker_);
  }

  void SetSourceTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    base::AutoLock locker(source_task_runner_lock_);
    source_task_runner_ = std::move(task_runner);
  }

  // webrtc::FrameTransformerInterface
  void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback,
      uint32_t ssrc) override {
    transformer_broker_->RegisterTransformedFrameSinkCallback(
        std::move(send_frame_to_sink_callback), ssrc);
  }

  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
    transformer_broker_->UnregisterTransformedFrameSinkCallback(ssrc);
  }

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
    auto video_frame =
        base::WrapUnique(static_cast<webrtc::TransformableVideoFrameInterface*>(
            frame.release()));
    if (metronome_) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
      queued_frames_.emplace_back(std::move(video_frame));
      if (!tick_scheduled_) {
        // Using a lambda here instead of a OnceClosure as
        // RequestCallOnNextTick() requires an absl::AnyInvocable.
        metronome_->RequestCallOnNextTick(
            [delegate = weak_factory_.GetWeakPtr()] {
              if (delegate) {
                delegate->InvokeQueuedTransforms();
              }
            });
        tick_scheduled_ = true;
      }
    } else {
      base::AutoLock locker(source_task_runner_lock_);
      PostCrossThreadTask(
          *source_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::Broker::
                                  TransformFrameOnSourceTaskRunner,
                              transformer_broker_, std::move(video_frame)));
    }
  }

 private:
  void InvokeQueuedTransforms() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(metronome_sequence_checker_);
    base::AutoLock locker(source_task_runner_lock_);
    tick_scheduled_ = false;
    for (std::unique_ptr<webrtc::TransformableVideoFrameInterface>& frame :
         queued_frames_) {
      PostCrossThreadTask(
          *source_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::Broker::
                                  TransformFrameOnSourceTaskRunner,
                              transformer_broker_, std::move(frame)));
    }
    queued_frames_.clear();
  }

  base::Lock source_task_runner_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> source_task_runner_
      GUARDED_BY(source_task_runner_lock_);
  scoped_refptr<RTCEncodedVideoStreamTransformer::Broker> transformer_broker_;

  std::unique_ptr<Metronome> metronome_;
  SEQUENCE_CHECKER(metronome_sequence_checker_);
  bool tick_scheduled_ GUARDED_BY_CONTEXT(metronome_sequence_checker_) = false;
  Vector<std::unique_ptr<webrtc::TransformableVideoFrameInterface>>
      queued_frames_ GUARDED_BY_CONTEXT(metronome_sequence_checker_);

  base::WeakPtrFactory<RTCEncodedVideoStreamTransformerDelegate> weak_factory_{
      this};
};

}  // namespace

RTCEncodedVideoStreamTransformer::Broker::Broker(
    RTCEncodedVideoStreamTransformer* transformer_)
    : transformer_(transformer_) {}

void RTCEncodedVideoStreamTransformer::Broker::
    RegisterTransformedFrameSinkCallback(
        rtc::scoped_refptr<webrtc::TransformedFrameCallback>
            send_frame_to_sink_callback,
        uint32_t ssrc) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->RegisterTransformedFrameSinkCallback(
        std::move(send_frame_to_sink_callback), ssrc);
  }
}

void RTCEncodedVideoStreamTransformer::Broker::
    UnregisterTransformedFrameSinkCallback(uint32_t ssrc) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->UnregisterTransformedFrameSinkCallback(ssrc);
  }
}

void RTCEncodedVideoStreamTransformer::Broker::TransformFrameOnSourceTaskRunner(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->TransformFrame(std::move(frame));
  }
}

void RTCEncodedVideoStreamTransformer::Broker::SetTransformerCallback(
    TransformerCallback callback) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SetTransformerCallback(std::move(callback));
  }
}

void RTCEncodedVideoStreamTransformer::Broker::ResetTransformerCallback() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->ResetTransformerCallback();
  }
}

void RTCEncodedVideoStreamTransformer::Broker::SetSourceTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SetSourceTaskRunner(std::move(task_runner));
  }
}

void RTCEncodedVideoStreamTransformer::Broker::ClearTransformer() {
  base::AutoLock locker(transformer_lock_);
  transformer_ = nullptr;
}

void RTCEncodedVideoStreamTransformer::Broker::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SendFrameToSink(std::move(frame));
  }
}

void RTCEncodedVideoStreamTransformer::Broker::StartShortCircuiting() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->StartShortCircuiting();
  }
}

RTCEncodedVideoStreamTransformer::RTCEncodedVideoStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner,
    std::unique_ptr<Metronome> metronome)
    : broker_(base::AdoptRef(new Broker(this))),
      delegate_(
          new rtc::RefCountedObject<RTCEncodedVideoStreamTransformerDelegate>(
              std::move(realm_task_runner),
              broker_,
              std::move(metronome))) {}

RTCEncodedVideoStreamTransformer::~RTCEncodedVideoStreamTransformer() {
  broker_->ClearTransformer();
}

void RTCEncodedVideoStreamTransformer::RegisterTransformedFrameSinkCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
    uint32_t ssrc) {
  base::AutoLock locker(sink_lock_);

  if (short_circuit_) {
    callback->StartShortCircuiting();
  }
  for (auto& sink_callback : send_frame_to_sink_callbacks_) {
    if (sink_callback.first == ssrc) {
      sink_callback.second = std::move(callback);
      return;
    }
  }
  send_frame_to_sink_callbacks_.push_back(std::make_pair(ssrc, callback));
}

void RTCEncodedVideoStreamTransformer::UnregisterTransformedFrameSinkCallback(
    uint32_t ssrc) {
  base::AutoLock locker(sink_lock_);
  for (wtf_size_t i = 0; i < send_frame_to_sink_callbacks_.size(); ++i) {
    if (send_frame_to_sink_callbacks_[i].first == ssrc) {
      send_frame_to_sink_callbacks_.EraseAt(i);
      return;
    }
  }
}

void RTCEncodedVideoStreamTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  base::AutoLock locker(source_lock_);
  // If no transformer callback has been set, drop the frame.
  if (!transformer_callback_)
    return;
  transformer_callback_.Run(std::move(frame));
}

void RTCEncodedVideoStreamTransformer::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  base::AutoLock locker(sink_lock_);
  if (send_frame_to_sink_callbacks_.size() == 1) {
    // Only a single sink callback registered, so this frame must use it.
    send_frame_to_sink_callbacks_[0].second->OnTransformedFrame(
        std::move(frame));
    return;
  }
  // Multiple sink callbacks registered, eg for simulcast. Find the correct
  // callback based on the ssrc of the written frame.
  for (const auto& sink_callback : send_frame_to_sink_callbacks_) {
    if (sink_callback.first == frame->GetSsrc()) {
      sink_callback.second->OnTransformedFrame(std::move(frame));
      return;
    }
  }
}

void RTCEncodedVideoStreamTransformer::StartShortCircuiting() {
  base::AutoLock locker(sink_lock_);
  short_circuit_ = true;
  for (const auto& sink_callback : send_frame_to_sink_callbacks_) {
    sink_callback.second->StartShortCircuiting();
  }
}

void RTCEncodedVideoStreamTransformer::SetTransformerCallback(
    TransformerCallback callback) {
  base::AutoLock locker(source_lock_);
  DCHECK(!transformer_callback_);
  transformer_callback_ = std::move(callback);
}

void RTCEncodedVideoStreamTransformer::ResetTransformerCallback() {
  base::AutoLock locker(source_lock_);
  transformer_callback_.Reset();
}

bool RTCEncodedVideoStreamTransformer::HasTransformerCallback() {
  base::AutoLock locker(source_lock_);
  return !!transformer_callback_;
}

bool RTCEncodedVideoStreamTransformer::HasTransformedFrameSinkCallback(
    uint32_t ssrc) const {
  base::AutoLock locker(sink_lock_);
  for (const auto& sink_callbacks : send_frame_to_sink_callbacks_) {
    if (sink_callbacks.first == ssrc)
      return true;
  }
  return false;
}

rtc::scoped_refptr<webrtc::FrameTransformerInterface>
RTCEncodedVideoStreamTransformer::Delegate() {
  return delegate_;
}

void RTCEncodedVideoStreamTransformer::SetSourceTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner) {
  static_cast<RTCEncodedVideoStreamTransformerDelegate*>(delegate_.get())
      ->SetSourceTaskRunner(std::move(realm_task_runner));
}

scoped_refptr<RTCEncodedVideoStreamTransformer::Broker>
RTCEncodedVideoStreamTransformer::GetBroker() {
  return broker_;
}

}  // namespace blink
