// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_client_info.h"
#include "content/public/browser/child_process_host.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo()
    : type_(blink::mojom::ServiceWorkerClientType::kWindow) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const blink::DedicatedWorkerToken& dedicated_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kDedicatedWorker),
      worker_token_(dedicated_worker_token) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const blink::SharedWorkerToken& shared_worker_token)
    : type_(blink::mojom::ServiceWorkerClientType::kSharedWorker),
      worker_token_(shared_worker_token) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const DedicatedOrSharedWorkerToken& worker_token)
    : worker_token_(worker_token) {
  if (worker_token.Is<blink::DedicatedWorkerToken>()) {
    type_ = blink::mojom::ServiceWorkerClientType::kDedicatedWorker;
  } else {
    DCHECK(worker_token.Is<blink::SharedWorkerToken>());
    type_ = blink::mojom::ServiceWorkerClientType::kSharedWorker;
  }
}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo& ServiceWorkerClientInfo::operator=(
    const ServiceWorkerClientInfo& other) = default;

ServiceWorkerClientInfo::~ServiceWorkerClientInfo() = default;

GlobalRenderFrameHostId ServiceWorkerClientInfo::GetRenderFrameHostId() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  return render_frame_host_id_;
}

void ServiceWorkerClientInfo::SetRenderFrameHostId(
    const GlobalRenderFrameHostId& render_frame_host_id) {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kWindow);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, render_frame_host_id.child_id);
  DCHECK_NE(MSG_ROUTING_NONE, render_frame_host_id.frame_routing_id);

  render_frame_host_id_ = render_frame_host_id;
}

blink::DedicatedWorkerToken ServiceWorkerClientInfo::GetDedicatedWorkerToken()
    const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kDedicatedWorker);
  return worker_token_->GetAs<blink::DedicatedWorkerToken>();
}

blink::SharedWorkerToken ServiceWorkerClientInfo::GetSharedWorkerToken() const {
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerClientType::kSharedWorker);
  return worker_token_->GetAs<blink::SharedWorkerToken>();
}

}  // namespace content
