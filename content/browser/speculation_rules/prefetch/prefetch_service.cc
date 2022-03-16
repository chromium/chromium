// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "url/gurl.h"

namespace content {

// static
std::unique_ptr<PrefetchService> PrefetchService::CreateIfPossible() {
  if (!base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor))
    return nullptr;

  return std::make_unique<PrefetchService>();
}

PrefetchService::PrefetchService() = default;

PrefetchService::~PrefetchService() = default;

void PrefetchService::PrefetchUrl(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  auto prefetch_container_key = prefetch_container->GetPrefetchContainerKey();

  DCHECK(all_prefetches_.find(prefetch_container_key) == all_prefetches_.end());
  all_prefetches_[prefetch_container_key] = prefetch_container;

  // TODO(https://crbug.com/1299059): Start eligibility check for
  // |prefetch_container|.
}

}  // namespace content
