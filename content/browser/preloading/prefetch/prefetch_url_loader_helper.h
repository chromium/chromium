// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_HELPER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

class PrefetchContainer;

// Checks if `prefetch_container` can be used for `tentative_resource_request`,
// and starts checking `PrefetchOriginProber` if needed.
void CONTENT_EXPORT
OnGotPrefetchToServe(int frame_tree_node_id,
                     const network::ResourceRequest& tentative_resource_request,
                     base::OnceCallback<void(base::WeakPtr<PrefetchContainer>)>
                         get_prefetch_callback,
                     base::WeakPtr<PrefetchContainer> prefetch_container);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_HELPER_H_
