// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_
#define CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_

#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class StoragePartition;

// A class used to make an asynchronous Mojo call with cloned patterns for each
// StoragePartition iteration. |this| instance will be destructed when all
// existing asynchronous Mojo calls made in SetLists() are done, and |closure|
// will be invoked on destructing |this|.
//
// Typically this would be used to implement
// BrowserContext::SetCorsOriginAccessListForOrigin, and would use
// ForEachStoragePartition with SetLists as the StoragePartitionCallback.
class CONTENT_EXPORT CorsOriginPatternSetter
    : public base::RefCounted<CorsOriginPatternSetter> {
 public:
  CorsOriginPatternSetter(
      const url::Origin& source_origin,
      std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
      std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
      base::OnceClosure closure);

  void ApplyToEachStoragePartition(BrowserContext* browser_context);

  static std::vector<network::mojom::CorsOriginPatternPtr> ClonePatterns(
      const std::vector<network::mojom::CorsOriginPatternPtr>& patterns);

 private:
  friend class base::RefCounted<CorsOriginPatternSetter>;

  void SetLists(StoragePartition* partition);
  ~CorsOriginPatternSetter();

  const url::Origin source_origin_;
  const std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns_;
  const std::vector<network::mojom::CorsOriginPatternPtr> block_patterns_;

  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(CorsOriginPatternSetter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CORS_ORIGIN_PATTERN_SETTER_H_
