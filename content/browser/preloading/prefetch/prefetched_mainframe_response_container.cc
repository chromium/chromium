// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"

#include <memory>
#include <string>

#include "net/base/isolation_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

PrefetchedMainframeResponseContainer::PrefetchedMainframeResponseContainer(
    const net::IsolationInfo& isolation_info,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body)
    : isolation_info_(isolation_info),
      head_(std::move(head)),
      body_(std::move(body)) {}

PrefetchedMainframeResponseContainer::~PrefetchedMainframeResponseContainer() =
    default;

network::mojom::URLResponseHeadPtr
PrefetchedMainframeResponseContainer::ReleaseHead() {
  DCHECK(head_);
  return std::move(head_);
}

std::unique_ptr<std::string>
PrefetchedMainframeResponseContainer::ReleaseBody() {
  DCHECK(body_);
  return std::move(body_);
}

}  // namespace content
