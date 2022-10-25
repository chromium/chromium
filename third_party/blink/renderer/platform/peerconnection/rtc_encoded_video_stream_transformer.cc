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
  // TODO(crbug.com/1065838): Remove the non-ssrc version of the registration
  // and unregistration methods once WebRTC uses the ssrc version in all cases.
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback) override {
    transformer_broker_->RegisterTransformedFrameSinkCallback(
        std::move(send_frame_to_sink_callback), 0);
  }

  void UnregisterTransformedFrameCallback() override {
    transformer_broker_->UnregisterTransformedFrameSinkCallback(0);
  }

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
    base::AutoLock locker(source_task_runner_lock_);
    auto video_frame =
        base::WrapUnique(static_cast<webrtc::TransformableVideoFrameInterface*>(
            frame.release()));
    PostCrossThreadTask(
        *source_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::Broker::
                                TransformFrameOnSourceTaskRunner,
                            transformer_broker_, std::move(video_frame)));
  }

 private:
  base::Lock source_task_runner_lock_;
  scoped_refptr<base::SingleThreadTaskRunner> source_task_runner_
      GUARDED_BY(source_task_runner_lock_);
  scoped_refptr<RTCEncodedVideoStreamTransformer::Broker> transformer_broker_;
};

}  // namespace

RTCEncodedVideoStreamTransformer::Broker::Broker(
    RTCEncodedVideoStreamTransformer* transformer_)
    : transformer_(transformer_) {}

void RTCEncodedVideoStreamTransformer::Broker::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback>
        send_frame_to_sink_callback) {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->RegisterTransformedFrameCallback(
        std::move(send_frame_to_sink_callback));
  }
}

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
    UnregisterTransformedFrameCallback() {
  base::AutoLock locker(transformer_lock_);
  if (transformer_) {
    transformer_->UnregisterTransformedFrameCallback();
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

RTCEncodedVideoStreamTransformer::RTCEncodedVideoStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> realm_task_runner)
    : broker_(base::AdoptRef(new Broker(this))),
      delegate_(
          new rtc::RefCountedObject<RTCEncodedVideoStreamTransformerDelegate>(
              std::move(realm_task_runner),
              broker_)) {}

RTCEncodedVideoStreamTransformer::~RTCEncodedVideoStreamTransformer() {
  broker_->ClearTransformer();
}

void RTCEncodedVideoStreamTransformer::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
  RegisterTransformedFrameSinkCallback(callback, 0);
}

void RTCEncodedVideoStreamTransformer::UnregisterTransformedFrameCallback() {
  UnregisterTransformedFrameSinkCallback(0);
}

void RTCEncodedVideoStreamTransformer::RegisterTransformedFrameSinkCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
    uint32_t ssrc) {
  base::AutoLock locker(sink_lock_);
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
  // TODO(crbug.com/1069275): Remove this section once WebRTC reports ssrc in
  // all sink callback registrations.
  base::AutoLock locker(sink_lock_);
  if (send_frame_to_sink_callbacks_.size() == 1 &&
      send_frame_to_sink_callbacks_[0].first == 0) {
    send_frame_to_sink_callbacks_[0].second->OnTransformedFrame(
        std::move(frame));
    return;
  }
  for (const auto& sink_callback : send_frame_to_sink_callbacks_) {
    if (sink_callback.first == frame->GetSsrc()) {
      sink_callback.second->OnTransformedFrame(std::move(frame));
      return;
    }
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
