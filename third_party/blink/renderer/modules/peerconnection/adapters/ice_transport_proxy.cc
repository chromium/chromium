// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

IceTransportProxy::IceTransportProxy(
    LocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    Delegate* delegate,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory)
    : proxy_thread_(std::move(proxy_thread)),
      host_thread_(std::move(host_thread)),
      host_(nullptr, base::OnTaskRunnerDeleter(host_thread_)),
      delegate_(delegate),
      feature_handle_for_scheduler_(frame.GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebRTC,
          {SchedulingPolicy::DisableAggressiveThrottling(),
           SchedulingPolicy::DisableAlignWakeUps()})) {
  DCHECK(host_thread_);
  DCHECK(delegate_);
  DCHECK(adapter_factory);
  DCHECK(proxy_thread_->BelongsToCurrentThread());
  adapter_factory->InitializeOnMainThread(frame);
  // Wait to initialize the host until the weak_ptr_factory_ is initialized.
  // The IceTransportHost is constructed on the proxy thread but should only be
  // interacted with via PostTask to the host thread. The OnTaskRunnerDeleter
  // (configured above) will ensure it gets deleted on the host thread.
  host_.reset(new IceTransportHost(proxy_thread_, host_thread_,
                                   weak_ptr_factory_.GetWeakPtr()));
  PostCrossThreadTask(*host_thread_, FROM_HERE,
                      CrossThreadBindOnce(&IceTransportHost::Initialize,
                                          CrossThreadUnretained(host_.get()),
                                          std::move(adapter_factory)));
}

IceTransportProxy::~IceTransportProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Note: The IceTransportHost will be deleted on the host thread.
}

scoped_refptr<base::SingleThreadTaskRunner> IceTransportProxy::proxy_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return proxy_thread_;
}

scoped_refptr<base::SingleThreadTaskRunner> IceTransportProxy::host_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return host_thread_;
}

void IceTransportProxy::OnGatheringStateChanged(
    cricket::IceGatheringState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnGatheringStateChanged(new_state);
}

void IceTransportProxy::OnCandidateGathered(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnCandidateGathered(candidate);
}

void IceTransportProxy::OnStateChanged(webrtc::IceTransportState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnStateChanged(new_state);
}

void IceTransportProxy::OnSelectedCandidatePairChanged(
    const std::pair<cricket::Candidate, cricket::Candidate>&
        selected_candidate_pair) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_->OnSelectedCandidatePairChanged(selected_candidate_pair);
}

}  // namespace blink
