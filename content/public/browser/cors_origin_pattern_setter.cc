// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

CorsOriginPatternSetter::CorsOriginPatternSetter(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure)
    : source_origin_(source_origin),
      allow_patterns_(std::move(allow_patterns)),
      block_patterns_(std::move(block_patterns)),
      closure_(std::move(closure)) {}

CorsOriginPatternSetter::~CorsOriginPatternSetter() {
  std::move(closure_).Run();
}

void CorsOriginPatternSetter::ApplyToEachStoragePartition(
    BrowserContext* browser_context) {
  BrowserContext::ForEachStoragePartition(
      browser_context, base::BindRepeating(&CorsOriginPatternSetter::SetLists,
                                           base::RetainedRef(this)));
}

void CorsOriginPatternSetter::SetLists(StoragePartition* partition) {
  partition->GetNetworkContext()->SetCorsOriginAccessListsForOrigin(
      source_origin_, ClonePatterns(allow_patterns_),
      ClonePatterns(block_patterns_),
      base::BindOnce([](scoped_refptr<CorsOriginPatternSetter> setter) {},
                     base::RetainedRef(this)));
}

// static
std::vector<network::mojom::CorsOriginPatternPtr>
CorsOriginPatternSetter::ClonePatterns(
    const std::vector<network::mojom::CorsOriginPatternPtr>& patterns) {
  std::vector<network::mojom::CorsOriginPatternPtr> cloned_patterns;
  cloned_patterns.reserve(patterns.size());
  for (const auto& item : patterns)
    cloned_patterns.push_back(item.Clone());
  return cloned_patterns;
}

}  // namespace content
