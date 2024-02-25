// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cors_origin_pattern_setter.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

CorsOriginPatternSetter::CorsOriginPatternSetter(
    base::PassKey<CorsOriginPatternSetter> pass_key,
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

void CorsOriginPatternSetter::SetForStoragePartition(
    content::StoragePartition* partition) {
  partition->GetNetworkContext()->SetCorsOriginAccessListsForOrigin(
      source_origin_, mojo::Clone(allow_patterns_),
      mojo::Clone(block_patterns_),
      base::DoNothingWithBoundArgs(base::RetainedRef(this)));
}

// static
void CorsOriginPatternSetter::Set(
    content::BrowserContext* browser_context,
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  auto barrier_closure = BarrierClosure(2, std::move(closure));

  scoped_refptr<CorsOriginPatternSetter> setter =
      base::MakeRefCounted<CorsOriginPatternSetter>(
          PassKey(), source_origin, mojo::Clone(allow_patterns),
          mojo::Clone(block_patterns), barrier_closure);
  browser_context->ForEachLoadedStoragePartition(
      [&](StoragePartition* partition) {
        setter->SetForStoragePartition(partition);
      });

  // Keep the per-profile access list up to date so that we can use this to
  // restore NetworkContext settings at anytime, e.g. on restarting the
  // network service.
  browser_context->GetSharedCorsOriginAccessList()->SetForOrigin(
      source_origin, std::move(allow_patterns), std::move(block_patterns),
      barrier_closure);
}

}  // namespace content
