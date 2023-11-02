// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_FAKE_SUBRESOURCE_FILTER_H_
#define CONTENT_WEB_TEST_RENDERER_FAKE_SUBRESOURCE_FILTER_H_

#include <string>
#include <vector>

#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace blink {
class WebURL;
}  // namespace blink

namespace content {

class FakeSubresourceFilter : public blink::WebDocumentSubresourceFilter {
 public:
  explicit FakeSubresourceFilter(
      std::vector<std::string> disallowed_path_suffixes,
      bool block_resources);
  ~FakeSubresourceFilter() override;

  FakeSubresourceFilter(const FakeSubresourceFilter&) = delete;
  FakeSubresourceFilter& operator=(const FakeSubresourceFilter&) = delete;

  // blink::WebDocumentSubresourceFilter:
  LoadPolicy GetLoadPolicy(const blink::WebURL& resource_url,
                           blink::mojom::RequestContextType) override;
  LoadPolicy GetLoadPolicyForWebSocketConnect(
      const blink::WebURL& url) override;
  LoadPolicy GetLoadPolicyForWebTransportConnect(
      const blink::WebURL& url) override;
  void ReportDisallowedLoad() override;
  bool ShouldLogToConsole() override;

 private:
  LoadPolicy GetLoadPolicyImpl(const blink::WebURL& url);

  const std::vector<std::string> disallowed_path_suffixes_;
  const bool block_subresources_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_FAKE_SUBRESOURCE_FILTER_H_
