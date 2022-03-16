// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace content {

// Manages all prefetches within a single BrowserContext. Responsible for
// checking the eligibility of the prefetch, making the network request for the
// prefetch, and provide prefetched resources to URL loader interceptor when
// needed.
class CONTENT_EXPORT PrefetchService {
 public:
  static std::unique_ptr<PrefetchService> CreateIfPossible();

  PrefetchService();
  ~PrefetchService();

  PrefetchService(const PrefetchService&) = delete;
  const PrefetchService& operator=(const PrefetchService&) = delete;

  void PrefetchUrl(base::WeakPtr<PrefetchContainer> prefetch_container);

 private:
  std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>
      all_prefetches_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_
