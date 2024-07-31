// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

namespace blink {

InterceptingNetworkController::FeedbackReceiver::FeedbackReceiver(
    CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : rtp_transport_handle_(rtp_transport_handle),
      task_runner_(std::move(task_runner)) {}

void InterceptingNetworkController::FeedbackReceiver::OnFeedback(
    webrtc::TransportPacketsFeedback feedback) {
  // Called on a WebRTC thread.
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &InterceptingNetworkController::FeedbackReceiver::
              OnFeedbackOnDestinationTaskRunner,
          WrapRefCounted(this), feedback,
          MakeUnwrappingCrossThreadWeakHandle(rtp_transport_handle_)));
}

void InterceptingNetworkController::FeedbackReceiver::
    OnFeedbackOnDestinationTaskRunner(webrtc::TransportPacketsFeedback feedback,
                                      RTCRtpTransport* rtp_transport) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (rtp_transport) {
    rtp_transport->OnFeedback(feedback);
  }
}

void InterceptingNetworkController::FeedbackReceiver::OnSentPacket(
    webrtc::SentPacket sp) {
  // Called on a WebRTC thread.
  CHECK(!task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &InterceptingNetworkController::FeedbackReceiver::
              OnSentPacketOnDestinationTaskRunner,
          WrapRefCounted(this), sp,
          MakeUnwrappingCrossThreadWeakHandle(rtp_transport_handle_)));
}

void InterceptingNetworkController::FeedbackReceiver::
    OnSentPacketOnDestinationTaskRunner(webrtc::SentPacket sp,
                                        RTCRtpTransport* rtp_transport) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (rtp_transport) {
    rtp_transport->OnSentPacket(sp);
  }
}

}  // namespace blink
