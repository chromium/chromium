// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

void InterceptingNetworkController::FeedbackProviderImpl::OnFeedback(
    webrtc::TransportPacketsFeedback feedback) {
  // Called on a WebRTC thread.
  base::AutoLock mutex(processor_lock_);
  // TODO(crbug.com/345101934): Consider buffering these until the
  // processor_handle has been created and then replaying them.
  if (!rtp_transport_processor_handle_) {
    return;
  }
  CHECK(!rtp_transport_processor_task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *rtp_transport_processor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&InterceptingNetworkController::FeedbackProviderImpl::
                              OnFeedbackOnDestinationTaskRunner,
                          WrapRefCounted(this), feedback,
                          MakeUnwrappingCrossThreadWeakHandle(
                              *rtp_transport_processor_handle_)));
}

void InterceptingNetworkController::FeedbackProviderImpl::
    OnFeedbackOnDestinationTaskRunner(
        webrtc::TransportPacketsFeedback feedback,
        RTCRtpTransportProcessor* rtp_transport_processor) {
  // Runs on the task runner  matching the JS thread of rtp_transport_processor.
  if (rtp_transport_processor) {
    rtp_transport_processor->OnFeedback(feedback);
  }
}

void InterceptingNetworkController::FeedbackProviderImpl::OnSentPacket(
    webrtc::SentPacket sp) {
  // Called on a WebRTC thread.
  base::AutoLock mutex(processor_lock_);
  // TODO(crbug.com/345101934): Consider buffering these until the
  // processor_handle has been created and then replaying them
  if (!rtp_transport_processor_handle_) {
    return;
  }
  CHECK(!rtp_transport_processor_task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *rtp_transport_processor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&InterceptingNetworkController::FeedbackProviderImpl::
                              OnSentPacketOnDestinationTaskRunner,
                          WrapRefCounted(this), sp,
                          MakeUnwrappingCrossThreadWeakHandle(
                              *rtp_transport_processor_handle_)));
}

void InterceptingNetworkController::FeedbackProviderImpl::
    OnSentPacketOnDestinationTaskRunner(
        webrtc::SentPacket sp,
        RTCRtpTransportProcessor* rtp_transport_processor) {
  // Runs on the task runner matching the JS thread of rtp_transport_processor
  // ie a worker.
  if (rtp_transport_processor) {
    rtp_transport_processor->OnSentPacket(sp);
  }
}

void InterceptingNetworkController::FeedbackProviderImpl::SetProcessor(
    CrossThreadWeakHandle<RTCRtpTransportProcessor>
        rtp_transport_processor_handle,
    scoped_refptr<base::SequencedTaskRunner>
        rtp_transport_processor_task_runner) {
  // Called on the main JS thread owning the RTCRtpTransport instance.
  base::AutoLock mutex(processor_lock_);
  rtp_transport_processor_handle_.emplace(
      std::move(rtp_transport_processor_handle));
  rtp_transport_processor_task_runner_ =
      std::move(rtp_transport_processor_task_runner);
}

}  // namespace blink
