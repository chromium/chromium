// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/shared_cors_origin_access_list_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

scoped_refptr<SharedCorsOriginAccessList> SharedCorsOriginAccessList::Create() {
  return base::MakeRefCounted<SharedCorsOriginAccessListImpl>();
}

SharedCorsOriginAccessListImpl::SharedCorsOriginAccessListImpl()
    : SharedCorsOriginAccessList() {}

void SharedCorsOriginAccessListImpl::SetForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  origin_access_list_.SetAllowListForOrigin(source_origin, allow_patterns);
  origin_access_list_.SetBlockListForOrigin(source_origin, block_patterns);
  std::move(closure).Run();
}

const network::cors::OriginAccessList&
SharedCorsOriginAccessListImpl::GetOriginAccessList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return origin_access_list_;
}

SharedCorsOriginAccessListImpl::~SharedCorsOriginAccessListImpl() = default;

}  // namespace content
