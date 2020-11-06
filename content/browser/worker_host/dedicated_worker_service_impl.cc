// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_service_impl.h"

#include "base/stl_util.h"
#include "content/browser/worker_host/dedicated_worker_host.h"

namespace content {

DedicatedWorkerServiceImpl::DedicatedWorkerInfo::DedicatedWorkerInfo(
    int worker_process_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    DedicatedWorkerHost* host)
    : worker_process_id(worker_process_id),
      ancestor_render_frame_host_id(ancestor_render_frame_host_id),
      dedicated_worker_host(host) {}

DedicatedWorkerServiceImpl::DedicatedWorkerInfo::DedicatedWorkerInfo(
    const DedicatedWorkerInfo& info) = default;
DedicatedWorkerServiceImpl::DedicatedWorkerInfo&
DedicatedWorkerServiceImpl::DedicatedWorkerInfo::operator=(
    const DedicatedWorkerInfo& info) = default;

DedicatedWorkerServiceImpl::DedicatedWorkerInfo::~DedicatedWorkerInfo() =
    default;

DedicatedWorkerServiceImpl::DedicatedWorkerServiceImpl() = default;

DedicatedWorkerServiceImpl::~DedicatedWorkerServiceImpl() = default;

void DedicatedWorkerServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DedicatedWorkerServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DedicatedWorkerServiceImpl::EnumerateDedicatedWorkers(Observer* observer) {
  for (const auto& kv : dedicated_worker_infos_) {
    const blink::DedicatedWorkerToken& dedicated_worker_token = kv.first;
    const DedicatedWorkerInfo& dedicated_worker_info = kv.second;

    observer->OnWorkerCreated(
        dedicated_worker_token, dedicated_worker_info.worker_process_id,
        dedicated_worker_info.ancestor_render_frame_host_id);
    if (dedicated_worker_info.final_response_url) {
      observer->OnFinalResponseURLDetermined(
          dedicated_worker_token, *dedicated_worker_info.final_response_url);
    }
  }
}

void DedicatedWorkerServiceImpl::NotifyWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    DedicatedWorkerHost* host) {
  bool inserted =
      dedicated_worker_infos_
          .emplace(worker_token,
                   DedicatedWorkerInfo(worker_process_id,
                                       ancestor_render_frame_host_id, host))
          .second;
  DCHECK(inserted);

  for (Observer& observer : observers_) {
    observer.OnWorkerCreated(worker_token, worker_process_id,
                             ancestor_render_frame_host_id);
  }
}

void DedicatedWorkerServiceImpl::NotifyBeforeWorkerDestroyed(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    GlobalFrameRoutingId ancestor_render_frame_host_id) {
  size_t removed = dedicated_worker_infos_.erase(dedicated_worker_token);
  DCHECK_EQ(1u, removed);

  for (Observer& observer : observers_) {
    observer.OnBeforeWorkerDestroyed(dedicated_worker_token,
                                     ancestor_render_frame_host_id);
  }
}

void DedicatedWorkerServiceImpl::NotifyWorkerFinalResponseURLDetermined(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    const GURL& url) {
  auto it = dedicated_worker_infos_.find(dedicated_worker_token);
  DCHECK(it != dedicated_worker_infos_.end());

  it->second.final_response_url = url;

  for (Observer& observer : observers_)
    observer.OnFinalResponseURLDetermined(dedicated_worker_token, url);
}

bool DedicatedWorkerServiceImpl::HasToken(
    const blink::DedicatedWorkerToken& worker_token) const {
  return dedicated_worker_infos_.count(worker_token);
}

DedicatedWorkerHost*
DedicatedWorkerServiceImpl::GetDedicatedWorkerHostFromToken(
    const blink::DedicatedWorkerToken& dedicated_worker_token) const {
  auto it = dedicated_worker_infos_.find(dedicated_worker_token);
  if (it == dedicated_worker_infos_.end())
    return nullptr;
  return it->second.dedicated_worker_host;
}

}  // namespace content
