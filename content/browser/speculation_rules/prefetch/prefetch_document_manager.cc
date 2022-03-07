// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"

#include <vector>

#include "content/browser/browser_context_impl.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh) {}

PrefetchDocumentManager::~PrefetchDocumentManager() = default;

void PrefetchDocumentManager::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  // TODO(https://crbug.com/1299059): Filter candidates that can't be prefetched
  // and any duplicates, then start the process to prefetch the remaining
  // candidates.
}

void PrefetchDocumentManager::PrefetchUrl(const GURL& url) {
  // TODO(https://crbug.com/1299059): Track metrics about the prefetches, and
  // add a parameter to specify the type of prefetch.
  DCHECK(BrowserContextImpl::From(render_frame_host().GetBrowserContext())
             ->GetPrefetchService());
  BrowserContextImpl::From(render_frame_host().GetBrowserContext())
      ->GetPrefetchService()
      ->PrefetchUrl(url);
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
