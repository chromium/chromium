// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_
#define CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_

#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class SharedCorsOriginAccessList;
class StoragePartition;

// A class for mutating CORS examptions list associated with a BrowserContext:
// 1. the in-process list in BrowserContext::shared_cors_origin_access_list
// 2. the per-NetworkContext (i.e. per-StoragePartition) lists
class CONTENT_EXPORT CorsOriginPatternSetter
    : public base::RefCounted<CorsOriginPatternSetter> {
 public:
  // Sets |allow_patterns| and |block_patterns| for |source_origin| for the
  // |browser_context|.
  //
  // The new settings will be 1) set in the SharedCorsOriginAccessList from
  // |browser_context|'s GetSharedCorsOriginAccessList as well as 2) pushed to
  // all network::mojom::NetworkContexts associated with all the current
  // StoragePartitions of the |browser_context|.  |closure| will be called once
  // all the stores/pushes have been acked.
  static void Set(
      content::BrowserContext* browser_context,
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure);

  // The constructor is semi-private (CorsOriginPatternSetter should only be
  // constructed internally, within the implementation of the public Set
  // method).
  using PassKey = base::PassKey<CorsOriginPatternSetter>;
  CorsOriginPatternSetter(
      base::PassKey<CorsOriginPatternSetter> pass_key,
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure);

  CorsOriginPatternSetter(const CorsOriginPatternSetter&) = delete;
  CorsOriginPatternSetter& operator=(const CorsOriginPatternSetter&) = delete;

 private:
  friend class base::RefCounted<CorsOriginPatternSetter>;
  ~CorsOriginPatternSetter();

  void SetForStoragePartition(content::StoragePartition* partition);

  const url::Origin source_origin_;
  const std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns_;
  const std::vector<network::mojom::CorsOriginPatternPtr> block_patterns_;

  base::OnceClosure closure_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_
