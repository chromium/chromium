// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_FETCH_CONTEXT_H_

#include "third_party/blink/public/platform/web_worker_fetch_context.h"

namespace blink {

namespace mojom {
class SubresourceLoaderUpdater;
}  // namespace mojom

// Worker fetch context for service worker. This has a feature to update the
// subresource loader factories through the subresouruce loader updater in
// addition to WebWorkerFetchContext.
class WebServiceWorkerFetchContext : public WebWorkerFetchContext {
 public:
  virtual mojom::SubresourceLoaderUpdater* GetSubresourceLoaderUpdater() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_SERVICE_WORKER_WEB_SERVICE_WORKER_FETCH_CONTEXT_H_
