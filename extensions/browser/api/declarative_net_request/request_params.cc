// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_params.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace {
namespace flat_rule = url_pattern_index::flat;

// Returns whether the request to |url| is third party to its |document_origin|.
// TODO(crbug.com/696822): Look into caching this.
bool IsThirdPartyRequest(const GURL& url, const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      url, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsThirdPartyRequest(const url::Origin& origin,
                         const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      origin, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

content::GlobalRenderFrameHostId GetFrameRoutingId(
    content::RenderFrameHost* host) {
  if (!host)
    return content::GlobalRenderFrameHostId();

  return host->GetGlobalId();
}

bool DoEmbedderConditionsMatch(
    int tab_id,
    const flatbuffers::Vector<uint8_t>& conditions_buffer) {
#if DCHECK_IS_ON()
  // Verify that `conditions_buffer` corresponds to a valid Flatbuffer with
  // `flat::EmbedderConditions` as the root. Note: this is a sanity check and
  // not a security check. Consider the two cases:
  //  - For a file backed ruleset, we already verify the file checksum on
  //    ruleset load. So the nested flatbuffer shouldn't be corrupted. On-disk
  //    modification of stored artifacts is outside Chrome's security model
  //    anyway.
  //  - For a non-file backed (session-scoped) ruleset, the ruleset is only
  //    maintained in memory. Hence there shouldn't be corruption risk.
  flatbuffers::Verifier verifier(conditions_buffer.Data(),
                                 conditions_buffer.size());
  CHECK(verifier.VerifyBuffer<flat::EmbedderConditions>(
      kEmbedderConditionsBufferIdentifier));
#endif  // DCHECK_IS_ON()

  auto* embedder_conditions =
      flatbuffers::GetRoot<flat::EmbedderConditions>(conditions_buffer.Data());
  DCHECK(embedder_conditions);

  auto matches = [tab_id](const flatbuffers::Vector<int32_t>& sorted_tab_ids) {
    DCHECK(std::is_sorted(sorted_tab_ids.begin(), sorted_tab_ids.end()));
    return std::binary_search(sorted_tab_ids.begin(), sorted_tab_ids.end(),
                              tab_id);
  };

  if (embedder_conditions->tab_ids_included() &&
      !matches(*embedder_conditions->tab_ids_included())) {
    return false;
  }

  if (embedder_conditions->tab_ids_excluded() &&
      matches(*embedder_conditions->tab_ids_excluded())) {
    return false;
  }

  return true;
}

}  // namespace

RequestParams::RequestParams(const WebRequestInfo& info)
    : url(&info.url),
      first_party_origin(info.initiator.value_or(url::Origin())),
      element_type(GetElementType(info.web_request_type)),
      method(GetRequestMethod(info.url.SchemeIsHTTPOrHTTPS(), info.method)),
      parent_routing_id(info.parent_routing_id),
      embedder_conditions_matcher(base::BindRepeating(DoEmbedderConditionsMatch,
                                                      info.frame_data.tab_id)) {
  is_third_party = IsThirdPartyRequest(*url, first_party_origin);
}

RequestParams::RequestParams(content::RenderFrameHost* host,
                             bool is_post_navigation)
    : url(&host->GetLastCommittedURL()),
      method(is_post_navigation ? flat_rule::RequestMethod_POST
                                : flat_rule::RequestMethod_GET),
      parent_routing_id(GetFrameRoutingId(host->GetParentOrOuterDocument())) {
  if (host->GetParentOrOuterDocument()) {
    // Note the discrepancy with the WebRequestInfo constructor. For a
    // navigation request, we'd use the request initiator as the
    // |first_party_origin|. But here we use the origin of the parent frame.
    // This is the same as crbug.com/996998.
    first_party_origin =
        host->GetParentOrOuterDocument()->GetLastCommittedOrigin();
    element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  } else {
    first_party_origin = url::Origin();
    element_type = url_pattern_index::flat::ElementType_MAIN_FRAME;
  }
  is_third_party =
      IsThirdPartyRequest(host->GetLastCommittedOrigin(), first_party_origin);

  int window_id_unused = extension_misc::kUnknownWindowId;
  int tab_id = extension_misc::kUnknownTabId;
  ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
      content::WebContents::FromRenderFrameHost(host), &tab_id,
      &window_id_unused);
  embedder_conditions_matcher =
      base::BindRepeating(DoEmbedderConditionsMatch, tab_id);
}

RequestParams::RequestParams(
    const GURL& url,
    const url::Origin& initiator,
    const api::declarative_net_request::ResourceType request_type,
    const api::declarative_net_request::RequestMethod request_method,
    int tab_id)
    : url(&url),
      first_party_origin(initiator),
      element_type(GetElementType(request_type)),
      is_third_party(IsThirdPartyRequest(url, first_party_origin)),
      method(GetRequestMethod(url.SchemeIsHTTPOrHTTPS(), request_method)),
      embedder_conditions_matcher(
          base::BindRepeating(DoEmbedderConditionsMatch, tab_id)) {}

RequestParams::RequestParams() = default;
RequestParams::~RequestParams() = default;

}  // namespace declarative_net_request
}  // namespace extensions
