// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/shared_cors_origin_access_list_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/features.h"

namespace content {

SharedCorsOriginAccessListImpl::SharedCorsOriginAccessListImpl()
    : SharedCorsOriginAccessList() {}

void SharedCorsOriginAccessListImpl::SetForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SharedCorsOriginAccessListImpl::SetForOriginOnIOThread,
                     base::RetainedRef(this), source_origin,
                     std::move(allow_patterns), std::move(block_patterns)),
      std::move(closure));
}

const network::cors::OriginAccessList&
SharedCorsOriginAccessListImpl::GetOriginAccessList() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return origin_access_list_;
}

SharedCorsOriginAccessListImpl::~SharedCorsOriginAccessListImpl() = default;

void SharedCorsOriginAccessListImpl::SetForOriginOnIOThread(
    const url::Origin source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  origin_access_list_.SetAllowListForOrigin(source_origin, allow_patterns);
  origin_access_list_.SetBlockListForOrigin(source_origin, block_patterns);
}

}  // namespace content
