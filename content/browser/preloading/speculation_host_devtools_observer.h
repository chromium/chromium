// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_HOST_DEVTOOLS_OBSERVER_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_HOST_DEVTOOLS_OBSERVER_H_

#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
class CONTENT_EXPORT SpeculationHostDevToolsObserver {
 public:
  virtual void OnStartSinglePrefetch(
      const std::string& request_id,
      const network::ResourceRequest& request,
      std::optional<
          std::pair<const GURL&,
                    const network::mojom::URLResponseHeadDevToolsInfo&>>
          redirect_info) = 0;
  virtual void OnPrefetchResponseReceived(
      const GURL& url,
      const std::string& request_id,
      const network::mojom::URLResponseHead& response) = 0;
  virtual void OnPrefetchRequestComplete(
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status) = 0;
  virtual void OnPrefetchBodyDataReceived(const std::string& request_id,
                                          const std::string& body,
                                          bool is_base64_encoded) = 0;
  virtual mojo::PendingRemote<network::mojom::DevToolsObserver>
  MakeSelfOwnedNetworkServiceDevToolsObserver() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_HOST_DEVTOOLS_OBSERVER_H_
