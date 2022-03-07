// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_service.h"

#include "base/feature_list.h"
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

void PrefetchService::PrefetchUrl(const GURL& url) {
  // TODO(https://crbug.com/1299059): Prefetch submitted URLs.
}

}  // namespace content
