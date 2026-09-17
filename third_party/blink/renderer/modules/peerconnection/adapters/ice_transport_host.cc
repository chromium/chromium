// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_host.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/api/transport/enums.h"
#include "third_party/webrtc/p2p/base/ice_transport_internal.h"

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
    webrtc::IceGatheringState new_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportProxy::OnGatheringStateChanged, proxy_,
                          new_state));
}

void IceTransportHost::OnCandidateGathered(const webrtc::Candidate& candidate) {
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
    const std::pair<webrtc::Candidate, webrtc::Candidate>&
        selected_candidate_pair) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *proxy_thread_, FROM_HERE,
      CrossThreadBindOnce(&IceTransportProxy::OnSelectedCandidatePairChanged,
                          proxy_, selected_candidate_pair));
}

}  // namespace blink
