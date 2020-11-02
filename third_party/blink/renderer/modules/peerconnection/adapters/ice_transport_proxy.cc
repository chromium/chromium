// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
           SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
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
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportHost::Initialize,
                          CrossThreadUnretained(host_.get()),
                          WTF::Passed(std::move(adapter_factory))));
}

IceTransportProxy::~IceTransportProxy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!HasConsumer());
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

void IceTransportProxy::StartGathering(
    const cricket::IceParameters& local_parameters,
    const cricket::ServerAddresses& stun_servers,
    const WebVector<cricket::RelayServerConfig>& turn_servers,
    IceTransportPolicy policy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportHost::StartGathering,
                          CrossThreadUnretained(host_.get()), local_parameters,
                          stun_servers, turn_servers, policy));
}

void IceTransportProxy::Start(
    const cricket::IceParameters& remote_parameters,
    cricket::IceRole role,
    const Vector<cricket::Candidate>& initial_remote_candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportHost::Start,
                          CrossThreadUnretained(host_.get()), remote_parameters,
                          role, initial_remote_candidates));
}

void IceTransportProxy::HandleRemoteRestart(
    const cricket::IceParameters& new_remote_parameters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportHost::HandleRemoteRestart,
                          CrossThreadUnretained(host_.get()),
                          new_remote_parameters));
}

void IceTransportProxy::AddRemoteCandidate(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *host_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportHost::AddRemoteCandidate,
                          CrossThreadUnretained(host_.get()), candidate));
}

bool IceTransportProxy::HasConsumer() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return consumer_proxy_;
}

IceTransportHost* IceTransportProxy::ConnectConsumer(
    QuicTransportProxy* consumer_proxy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(consumer_proxy);
  DCHECK(!consumer_proxy_);
  consumer_proxy_ = consumer_proxy;
  return host_.get();
}

void IceTransportProxy::DisconnectConsumer(QuicTransportProxy* consumer_proxy) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(consumer_proxy);
  DCHECK_EQ(consumer_proxy, consumer_proxy_);
  consumer_proxy_ = nullptr;
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
