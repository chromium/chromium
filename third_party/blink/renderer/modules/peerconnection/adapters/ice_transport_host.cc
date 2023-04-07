// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

IceTransportHost::IceTransportHost(
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    base::WeakPtr<IceTransportProxy> proxy)
    : proxy_thread_(std::move(proxy_thread)),
      host_thread_(std::move(host_thread)),
      proxy_(std::move(proxy)) {
  DETACH_FROM_THREAD(thread_checker_);
  DCHECK(proxy_thread_);
  DCHECK(host_thread_);
  DCHECK(proxy_);
}

IceTransportHost::~IceTransportHost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void IceTransportHost::Initialize(
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(adapter_factory);
  transport_ = adapter_factory->ConstructOnWorkerThread(this);
}

scoped_refptr<base::SingleThreadTaskRunner> IceTransportHost::proxy_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return proxy_thread_;
}

scoped_refptr<base::SingleThreadTaskRunner> IceTransportHost::host_thread()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return host_thread_;
}

void IceTransportHost::OnGatheringStateChanged(
    cricket::IceGatheringState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportProxy::OnGatheringStateChanged, proxy_,
                          new_state));
}

void IceTransportHost::OnCandidateGathered(
    const cricket::Candidate& candidate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportProxy::OnCandidateGathered, proxy_,
                          candidate));
}

void IceTransportHost::OnStateChanged(webrtc::IceTransportState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(*proxy_thread_, FROM_HERE,
                      CrossThreadBindOnce(&IceTransportProxy::OnStateChanged,
                                          proxy_, new_state));
}

void IceTransportHost::OnSelectedCandidatePairChanged(
    const std::pair<cricket::Candidate, cricket::Candidate>&
        selected_candidate_pair) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportProxy::OnSelectedCandidatePairChanged,
                          proxy_, selected_candidate_pair));
}

}  // namespace blink
