// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/test/service_worker_registration_waiter.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

namespace web_app {

ServiceWorkerRegistrationWaiter::ServiceWorkerRegistrationWaiter(
    content::BrowserContext* browser_context,
    const GURL& url)
    : ServiceWorkerRegistrationWaiter(
          browser_context->GetStoragePartitionForUrl(url),
          url) {}

ServiceWorkerRegistrationWaiter::ServiceWorkerRegistrationWaiter(
    content::StoragePartition* storage_partition,
    const GURL& url)
    : url_(std::move(url)) {
  DCHECK(storage_partition);

  service_worker_context_ = storage_partition->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);
}

ServiceWorkerRegistrationWaiter::~ServiceWorkerRegistrationWaiter() {
  if (service_worker_context_) {
    service_worker_context_->RemoveObserver(this);
  }
}

void ServiceWorkerRegistrationWaiter::AwaitRegistration(
    const base::Location& location) {
  complete_run_loop_.Run(location);
}

void ServiceWorkerRegistrationWaiter::AwaitRegistrationStored(
    const base::Location& location) {
  stored_run_loop_.Run(location);
}

void ServiceWorkerRegistrationWaiter::OnRegistrationCompleted(
    const GURL& pattern) {
  if (content::ServiceWorkerContext::ScopeMatches(pattern, url_)) {
    complete_run_loop_.Quit();
  }
}

void ServiceWorkerRegistrationWaiter::OnRegistrationStored(
    int64_t registration_id,
    const GURL& scope) {
  if (content::ServiceWorkerContext::ScopeMatches(scope, url_)) {
    stored_run_loop_.Quit();
  }
}

void ServiceWorkerRegistrationWaiter::OnDestruct(
    content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

}  // namespace web_app
