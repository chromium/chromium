// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
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
// rtc::RefCountedObject and posts tasks referencing the delegate, which invokes
// the RTCEncodedVideoStreamTransformer via callbacks.
class RTCEncodedVideoStreamTransformerDelegate
    : public webrtc::FrameTransformerInterface {
 public:
  RTCEncodedVideoStreamTransformerDelegate(
      const base::WeakPtr<RTCEncodedVideoStreamTransformer>& transformer,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : transformer_(transformer),
        main_task_runner_(std::move(main_task_runner)) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }

  // webrtc::FrameTransformerInterface
  // TODO(crbug.com/1065838): Remove the non-ssrc version of the registration
  // and unregistration methods once WebRTC uses the ssrc version in all cases.
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback) override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::
                                RegisterTransformedFrameSinkCallback,
                            transformer_,
                            std::move(send_frame_to_sink_callback), 0));
  }

  void UnregisterTransformedFrameCallback() override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::
                                UnregisterTransformedFrameSinkCallback,
                            transformer_, 0));
  }

  void RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback,
      uint32_t ssrc) override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::
                                RegisterTransformedFrameSinkCallback,
                            transformer_,
                            std::move(send_frame_to_sink_callback), ssrc));
  }

  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::
                                UnregisterTransformedFrameSinkCallback,
                            transformer_, ssrc));
  }

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
    auto video_frame =
        base::WrapUnique(static_cast<webrtc::TransformableVideoFrameInterface*>(
            frame.release()));
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::TransformFrame,
                            transformer_, std::move(video_frame)));
  }

 private:
  base::WeakPtr<RTCEncodedVideoStreamTransformer> transformer_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

RTCEncodedVideoStreamTransformer::RTCEncodedVideoStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  DCHECK(main_task_runner->BelongsToCurrentThread());
  delegate_ =
      new rtc::RefCountedObject<RTCEncodedVideoStreamTransformerDelegate>(
          weak_factory_.GetWeakPtr(), std::move(main_task_runner));
}

void RTCEncodedVideoStreamTransformer::RegisterTransformedFrameSinkCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
    uint32_t ssrc) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (wtf_size_t i = 0; i < send_frame_to_sink_callbacks_.size(); ++i) {
    if (send_frame_to_sink_callbacks_[i].first == ssrc) {
      send_frame_to_sink_callbacks_.EraseAt(i);
      return;
    }
  }
}

void RTCEncodedVideoStreamTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If no transformer callback has been set, drop the frame.
  if (!transformer_callback_)
    return;

  transformer_callback_.Run(std::move(frame));
}

void RTCEncodedVideoStreamTransformer::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(crbug.com/1069275): Remove this section once WebRTC reports ssrc in
  // all sink callback registrations.
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_ = std::move(callback);
}

void RTCEncodedVideoStreamTransformer::ResetTransformerCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_.Reset();
}

bool RTCEncodedVideoStreamTransformer::HasTransformerCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !transformer_callback_.is_null();
}

bool RTCEncodedVideoStreamTransformer::HasTransformedFrameSinkCallback(
    uint32_t ssrc) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& sink_callbacks : send_frame_to_sink_callbacks_) {
    if (sink_callbacks.first == ssrc)
      return true;
  }
  return false;
}

rtc::scoped_refptr<webrtc::FrameTransformerInterface>
RTCEncodedVideoStreamTransformer::Delegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return delegate_;
}

}  // namespace blink
