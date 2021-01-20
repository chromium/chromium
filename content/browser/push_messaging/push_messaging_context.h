// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONTEXT_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONTEXT_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"

class GURL;

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

// Observes the service worker context of the storage partition owning this
// instance and informs the push service of relevant service worker events.
class PushMessagingContext : public ServiceWorkerContextCoreObserver {
 public:
  PushMessagingContext(
      BrowserContext* browser_context,
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);
  ~PushMessagingContext() override;

  // ServiceWorkerContextCoreObserver methods
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& pattern) override;
  void OnStorageWiped() override;

 private:
  BrowserContext* browser_context_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_CONTEXT_H_
