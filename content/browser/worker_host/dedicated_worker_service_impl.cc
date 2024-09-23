// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_service_impl.h"

#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

DedicatedWorkerServiceImpl::DedicatedWorkerServiceImpl() = default;

DedicatedWorkerServiceImpl::~DedicatedWorkerServiceImpl() = default;

void DedicatedWorkerServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DedicatedWorkerServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DedicatedWorkerServiceImpl::EnumerateDedicatedWorkers(Observer* observer) {
  for (const auto& kv : dedicated_worker_hosts_) {
    const blink::DedicatedWorkerToken& dedicated_worker_token = kv.first;
    DedicatedWorkerHost* host = kv.second;

    observer->OnWorkerCreated(
        dedicated_worker_token, host->GetProcessHost()->GetID(),
        host->GetStorageKey().origin(), host->GetCreator());
    auto& maybe_url = host->GetFinalResponseURL();
    if (maybe_url) {
      observer->OnFinalResponseURLDetermined(dedicated_worker_token,
                                             *maybe_url);
    }
  }
}

void DedicatedWorkerServiceImpl::NotifyWorkerCreated(
    DedicatedWorkerHost* host) {
  bool inserted =
      dedicated_worker_hosts_.emplace(host->GetToken(), host).second;
  DCHECK(inserted);

  for (Observer& observer : observers_) {
    observer.OnWorkerCreated(host->GetToken(), host->GetProcessHost()->GetID(),
                             host->GetStorageKey().origin(),
                             host->GetCreator());
  }
}

void DedicatedWorkerServiceImpl::NotifyBeforeWorkerDestroyed(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    DedicatedWorkerCreator creator) {
  size_t removed = dedicated_worker_hosts_.erase(dedicated_worker_token);
  DCHECK_EQ(1u, removed);

  for (Observer& observer : observers_) {
    observer.OnBeforeWorkerDestroyed(dedicated_worker_token, creator);
  }
}

void DedicatedWorkerServiceImpl::NotifyWorkerFinalResponseURLDetermined(
    const blink::DedicatedWorkerToken& dedicated_worker_token,
    const GURL& url) {
  auto it = dedicated_worker_hosts_.find(dedicated_worker_token);
  CHECK(it != dedicated_worker_hosts_.end(), base::NotFatalUntil::M130);

  for (Observer& observer : observers_) {
    observer.OnFinalResponseURLDetermined(dedicated_worker_token, url);
  }
}

bool DedicatedWorkerServiceImpl::HasToken(
    const blink::DedicatedWorkerToken& worker_token) const {
  return dedicated_worker_hosts_.count(worker_token);
}

DedicatedWorkerHost*
DedicatedWorkerServiceImpl::GetDedicatedWorkerHostFromToken(
    const blink::DedicatedWorkerToken& dedicated_worker_token) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = dedicated_worker_hosts_.find(dedicated_worker_token);
  if (it == dedicated_worker_hosts_.end())
    return nullptr;
  return it->second;
}

}  // namespace content
