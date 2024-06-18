// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"

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

// This delegate class exists to work around the fact that
// RTCEncodedAudioStreamTransformer cannot derive from rtc::RefCountedObject
// and post tasks referencing itself as an rtc::scoped_refptr. Instead,
// RTCEncodedAudioStreamTransformer creates a delegate using
// rtc::RefCountedObject and posts tasks referencing the delegate, which
// invokes the RTCEncodedAudioStreamTransformer via callbacks.
class RTCEncodedAudioStreamTransformerDelegate
    : public webrtc::FrameTransformerInterface {
 public:
  RTCEncodedAudioStreamTransformerDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner,
      scoped_refptr<RTCEncodedAudioStreamTransformer::Broker>
          transformer_broker)
      : source_task_runner_(realm_task_runner),
        transformer_broker_(std::move(transformer_broker)) {
    DCHECK(source_task_runner_->BelongsToCurrentThread());
  }

  void SetSourceTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    base::AutoLock locker(source_task_runner_lock_);
    source_task_runner_ = std::move(task_runner);
  }

  // webrtc::FrameTransformerInterface
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback) override {
    transformer_broker_->RegisterTransformedFrameCallback(
        std::move(send_frame_to_sink_callback));
  }

  void UnregisterTransformedFrameCallback() override {
    transformer_broker_->UnregisterTransformedFrameCallback();
  }

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
    base::AutoLock locker(source_task_runner_lock_);
    auto audio_frame =
        base::WrapUnique(static_cast<webrtc::TransformableAudioFrameInterface*>(
            frame.release()));
    PostCrossThreadTask(
        *source_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedAudioStreamTransformer::Broker::
                                TransformFrameOnSourceTaskRunner,
                            transformer_broker_, std::move(audio_frame)));
  }

 private:
  base::Lock source_task_runner_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> source_task_runner_
      GUARDED_BY(source_task_runner_lock_);
  scoped_refptr<RTCEncodedAudioStreamTransformer::Broker> transformer_broker_;
};

}  // namespace

RTCEncodedAudioStreamTransformer::Broker::Broker(
    RTCEncodedAudioStreamTransformer* transformer_)
    : transformer_(transformer_) {}

void RTCEncodedAudioStreamTransformer::Broker::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback>
        send_frame_to_sink_callback) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->RegisterTransformedFrameCallback(
        std::move(send_frame_to_sink_callback));
  }
}

void RTCEncodedAudioStreamTransformer::Broker::
    UnregisterTransformedFrameCallback() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->UnregisterTransformedFrameCallback();
  }
}

void RTCEncodedAudioStreamTransformer::Broker::TransformFrameOnSourceTaskRunner(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->TransformFrame(std::move(frame));
  }
}

void RTCEncodedAudioStreamTransformer::Broker::SetTransformerCallback(
    TransformerCallback callback) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SetTransformerCallback(std::move(callback));
  }
}

void RTCEncodedAudioStreamTransformer::Broker::ResetTransformerCallback() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->ResetTransformerCallback();
  }
}

void RTCEncodedAudioStreamTransformer::Broker::SetSourceTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SetSourceTaskRunner(std::move(task_runner));
  }
}

void RTCEncodedAudioStreamTransformer::Broker::ClearTransformer() {
  base::AutoLock locker(transformer_lock_);
  transformer_ = nullptr;
}

void RTCEncodedAudioStreamTransformer::Broker::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->SendFrameToSink(std::move(frame));
  }
}

void RTCEncodedAudioStreamTransformer::Broker::StartShortCircuiting() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->StartShortCircuiting();
  }
}

RTCEncodedAudioStreamTransformer::RTCEncodedAudioStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner)
    : broker_(base::AdoptRef(new Broker(this))),
      delegate_(
          new rtc::RefCountedObject<RTCEncodedAudioStreamTransformerDelegate>(
              std::move(realm_task_runner),
              broker_)) {}

RTCEncodedAudioStreamTransformer::~RTCEncodedAudioStreamTransformer() {
  broker_->ClearTransformer();
}

void RTCEncodedAudioStreamTransformer::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
  base::AutoLock locker(sink_lock_);
  send_frame_to_sink_cb_ = callback;
  if (short_circuit_) {
    callback->StartShortCircuiting();
  }
}

void RTCEncodedAudioStreamTransformer::UnregisterTransformedFrameCallback() {
  base::AutoLock locker(sink_lock_);
  send_frame_to_sink_cb_ = nullptr;
}

void RTCEncodedAudioStreamTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
  base::AutoLock locker(source_lock_);
  // If no transformer callback has been set, drop the frame.
  if (!transformer_callback_)
    return;
  transformer_callback_.Run(std::move(frame));
}

void RTCEncodedAudioStreamTransformer::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> frame) {
  base::AutoLock locker(sink_lock_);
  if (send_frame_to_sink_cb_)
    send_frame_to_sink_cb_->OnTransformedFrame(std::move(frame));
}

void RTCEncodedAudioStreamTransformer::StartShortCircuiting() {
  base::AutoLock locker(sink_lock_);
  short_circuit_ = true;
  if (send_frame_to_sink_cb_) {
    send_frame_to_sink_cb_->StartShortCircuiting();
  }
}

void RTCEncodedAudioStreamTransformer::SetTransformerCallback(
    TransformerCallback callback) {
  base::AutoLock locker(source_lock_);
  transformer_callback_ = std::move(callback);
}

void RTCEncodedAudioStreamTransformer::ResetTransformerCallback() {
  base::AutoLock locker(source_lock_);
  transformer_callback_.Reset();
}

bool RTCEncodedAudioStreamTransformer::HasTransformerCallback() {
  base::AutoLock locker(source_lock_);
  return !!transformer_callback_;
}

bool RTCEncodedAudioStreamTransformer::HasTransformedFrameCallback() const {
  base::AutoLock locker(sink_lock_);
  return !!send_frame_to_sink_cb_;
}

rtc::scoped_refptr<webrtc::FrameTransformerInterface>
RTCEncodedAudioStreamTransformer::Delegate() {
  return delegate_;
}

void RTCEncodedAudioStreamTransformer::SetSourceTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner) {
  static_cast<RTCEncodedAudioStreamTransformerDelegate*>(delegate_.get())
      ->SetSourceTaskRunner(std::move(realm_task_runner));
}

scoped_refptr<RTCEncodedAudioStreamTransformer::Broker>
RTCEncodedAudioStreamTransformer::GetBroker() {
  return broker_;
}

}  // namespace blink
