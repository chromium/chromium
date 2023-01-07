// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prefetch_service_delegate.h"

#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/public/browser/browser_context.h"

namespace content {

// static
void PrefetchServiceDelegate::ClearData(BrowserContext* browser_context) {
  PrefetchService* prefetch_service =
      BrowserContextImpl::From(browser_context)->GetPrefetchService();
  if (!prefetch_service)
    return;

  PrefetchServiceDelegate* prefetch_service_delegate =
      prefetch_service->GetPrefetchServiceDelegate();
  if (!prefetch_service_delegate)
    return;

  prefetch_service_delegate->ClearData();
}

}  // namespace content