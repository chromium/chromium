// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_context.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "url/gurl.h"

namespace content {

PushMessagingContext::PushMessagingContext(
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : browser_context_(browser_context),
      service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_context_->AddObserver(this);
}

PushMessagingContext::~PushMessagingContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_context_->RemoveObserver(this);
}

void PushMessagingContext::OnRegistrationDeleted(int64_t registration_id,
                                                 const GURL& pattern,
                                                 const blink::StorageKey& key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service =
      browser_context_->GetPushMessagingService();
  if (push_service) {
    push_service->DidDeleteServiceWorkerRegistration(
        pattern.DeprecatedGetOriginAsURL(), registration_id);
  }
}

void PushMessagingContext::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service =
      browser_context_->GetPushMessagingService();
  if (push_service)
    push_service->DidDeleteServiceWorkerDatabase();
}

}  // namespace content
