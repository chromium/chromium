// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCHED_MAINFRAME_RESPONSE_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCHED_MAINFRAME_RESPONSE_CONTAINER_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

class CONTENT_EXPORT PrefetchedMainframeResponseContainer {
 public:
  PrefetchedMainframeResponseContainer(const net::IsolationInfo& info,
                                       network::mojom::URLResponseHeadPtr head,
                                       std::unique_ptr<std::string> body);
  ~PrefetchedMainframeResponseContainer();

  PrefetchedMainframeResponseContainer(
      const PrefetchedMainframeResponseContainer&) = delete;
  PrefetchedMainframeResponseContainer& operator=(
      const PrefetchedMainframeResponseContainer&) = delete;

  const net::IsolationInfo& isolation_info() { return isolation_info_; }

  // Returns reference to the response head.
  const network::mojom::URLResponseHead* GetHead() { return head_.get(); }

  // Releases the ownership of the response head from |this| and gives it to the
  // caller.
  network::mojom::URLResponseHeadPtr ReleaseHead();

  // Releases the ownership of the response body from |this| and gives it to the
  // caller.
  std::unique_ptr<std::string> ReleaseBody();

 private:
  const net::IsolationInfo isolation_info_;
  network::mojom::URLResponseHeadPtr head_;
  std::unique_ptr<std::string> body_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCHED_MAINFRAME_RESPONSE_CONTAINER_H_
