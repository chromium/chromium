// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/no_vary_search_helper.h"

#include <utility>

#include "content/public/browser/render_frame_host.h"
#include "services/network/public/cpp/no_vary_search_header_parser.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/origin.h"

namespace content {

namespace no_vary_search {

// static
net::HttpNoVarySearchData ParseHttpNoVarySearchDataFromMojom(
    const network::mojom::NoVarySearchPtr& no_vary_search_ptr) {
  if (no_vary_search_ptr->search_variance->is_vary_params()) {
    return net::HttpNoVarySearchData::CreateFromVaryParams(
        no_vary_search_ptr->search_variance->get_vary_params(),
        no_vary_search_ptr->vary_on_key_order);
  }
  return net::HttpNoVarySearchData::CreateFromNoVaryParams(
      no_vary_search_ptr->search_variance->get_no_vary_params(),
      no_vary_search_ptr->vary_on_key_order);
}

void SetNoVarySearchData(base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  // Check if the prefetched response has a No-Vary-Search header and
  // call SetNoVarySearchData() so that it can be looked up by
  // IterateCandidates().
  const network::mojom::URLResponseHead& head = *prefetch_container->GetHead();
  if (!(head.parsed_headers &&
        head.parsed_headers->no_vary_search_with_parse_error)) {
    return;
  }
  if (head.parsed_headers->no_vary_search_with_parse_error->is_parse_error()) {
    return;
  }
  auto no_vary_search_data = ParseHttpNoVarySearchDataFromMojom(
      head.parsed_headers->no_vary_search_with_parse_error
          ->get_no_vary_search());
  prefetch_container->SetNoVarySearchData(std::move(no_vary_search_data));
}

void MaybeSendErrorsToConsole(const GURL& url,
                              const network::mojom::URLResponseHead& head,
                              RenderFrameHost& rfh) {
  DCHECK(
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch));
  if (!(head.parsed_headers &&
        head.parsed_headers->no_vary_search_with_parse_error)) {
    return;
  }
  if (!head.parsed_headers->no_vary_search_with_parse_error->is_parse_error()) {
    return;
  }
  const auto parse_error =
      head.parsed_headers->no_vary_search_with_parse_error->get_parse_error();
  if (parse_error == network::mojom::NoVarySearchParseError::kOk) {
    return;
  }
  blink::mojom::ConsoleMessageLevel error_level =
      parse_error == network::mojom::NoVarySearchParseError::kDefaultValue
          ? blink::mojom::ConsoleMessageLevel::kWarning
          : blink::mojom::ConsoleMessageLevel::kError;
  auto error_message = network::GetNoVarySearchConsoleMessage(parse_error, url);
  CHECK(error_message);
  rfh.AddMessageToConsole(error_level, error_message.value());
}

}  // namespace no_vary_search

}  // namespace content
