// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"

#include "media/media_buildflags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/web_package/signed_exchange_consts.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

constexpr char kAlternate[] = "alternate";
constexpr char kAllowedAltSxg[] = "allowed-alt-sxg";

using AlternateSignedExchangeMachingKey =
    std::pair<String /* anchor */,
              std::pair<String /* variants */, String /* variant_key */>>;

AlternateSignedExchangeMachingKey MakeKey(const String& anchor,
                                          const String& variants,
                                          const String& variant_key) {
  // Null string can't be used as a key of HashMap.
  return std::make_pair(
      anchor.IsNull() ? "" : anchor,
      std::make_pair(variants.IsNull() ? "" : variants,
                     variant_key.IsNull() ? "" : variant_key));
}

void AddAlternateUrlIfValid(
    const LinkHeader& header,
    HashMap<AlternateSignedExchangeMachingKey, KURL>* alternate_urls) {
  if (!header.Valid() || header.Url().empty() || !header.Anchor().has_value() ||
      header.Anchor()->empty() ||
      !EqualIgnoringASCIICase(header.Rel(), kAlternate) ||
      header.MimeType() != kSignedExchangeMimeType) {
    return;
  }
  const KURL alternative_url(header.Url());
  const KURL anchor_url(*header.Anchor());
  if (!alternative_url.IsValid() || !anchor_url.IsValid())
    return;
  alternate_urls->Set(
      MakeKey(*header.Anchor(), header.Variants(), header.VariantKey()),
      alternative_url);
}

std::unique_ptr<AlternateSignedExchangeResourceInfo::Entry>
CreateEntryForLinkHeaderIfValid(
    const LinkHeader& header,
    const HashMap<AlternateSignedExchangeMachingKey, KURL>& alternate_urls) {
  if (!header.Valid() || header.Url().empty() ||
      header.HeaderIntegrity().empty() ||
      !EqualIgnoringASCIICase(header.Rel(), kAllowedAltSxg)) {
    return nullptr;
  }
  const KURL anchor_url(header.Url());
  if (!anchor_url.IsValid())
    return nullptr;

  KURL alternative_url;
  const auto alternate_urls_it = alternate_urls.find(
      MakeKey(header.Url(), header.Variants(), header.VariantKey()));
  if (alternate_urls_it != alternate_urls.end())
    alternative_url = alternate_urls_it->value;

  return std::make_unique<AlternateSignedExchangeResourceInfo::Entry>(
      anchor_url, alternative_url, header.HeaderIntegrity(), header.Variants(),
      header.VariantKey());
}

}  // namespace

std::unique_ptr<AlternateSignedExchangeResourceInfo>
AlternateSignedExchangeResourceInfo::CreateIfValid(
    const String& outer_link_header,
    const String& inner_link_header) {
  HashMap<AlternateSignedExchangeMachingKey, KURL> alternate_urls;
  const auto outer_link_headers = LinkHeaderSet(outer_link_header);
  for (const auto& header : outer_link_headers) {
    AddAlternateUrlIfValid(header, &alternate_urls);
  }

  EntryMap alternative_resources;
  const auto inner_link_headers = LinkHeaderSet(inner_link_header);
  for (const auto& header : inner_link_headers) {
    auto alt_resource = CreateEntryForLinkHeaderIfValid(header, alternate_urls);
    if (!alt_resource)
      continue;
    auto anchor_url = alt_resource->anchor_url();
    auto alternative_resources_it = alternative_resources.find(anchor_url);
    if (alternative_resources_it == alternative_resources.end()) {
      Vector<std::unique_ptr<Entry>> resource_list;
      resource_list.emplace_back(std::move(alt_resource));
      alternative_resources.Set(anchor_url, std::move(resource_list));
    } else {
      alternative_resources_it->value.emplace_back(std::move(alt_resource));
    }
  }
  if (alternative_resources.empty())
    return nullptr;
  return std::make_unique<AlternateSignedExchangeResourceInfo>(
      std::move(alternative_resources));
}

AlternateSignedExchangeResourceInfo::AlternateSignedExchangeResourceInfo(
    EntryMap alternative_resources)
    : alternative_resources_(std::move(alternative_resources)) {}

AlternateSignedExchangeResourceInfo::Entry*
AlternateSignedExchangeResourceInfo::FindMatchingEntry(
    const KURL& url,
    std::optional<ResourceType> resource_type,
    const Vector<String>& languages) const {
  return FindMatchingEntry(
      url,
      resource_type
          ? network_utils::GetAcceptHeaderForDestination(
                ResourceFetcher::DetermineRequestDestination(*resource_type))
          : network::kDefaultAcceptHeaderValue,
      languages);
}

AlternateSignedExchangeResourceInfo::Entry*
AlternateSignedExchangeResourceInfo::FindMatchingEntry(
    const KURL& url,
    network::mojom::RequestDestination request_destination,
    const Vector<String>& languages) const {
  return FindMatchingEntry(
      url, network_utils::GetAcceptHeaderForDestination(request_destination),
      languages);
}

AlternateSignedExchangeResourceInfo::Entry*
AlternateSignedExchangeResourceInfo::FindMatchingEntry(
    const KURL& url,
    const char* accept_header,
    const Vector<String>& languages) const {
  const auto it = alternative_resources_.find(url);
  if (it == alternative_resources_.end())
    return nullptr;
  const Vector<std::unique_ptr<Entry>>& entries = it->value;
  DCHECK(!entries.empty());
  if (entries[0]->variants().IsNull())
    return entries[0].get();

  const std::string variants = entries[0]->variants().Utf8();
  std::vector<std::string> variant_keys_list;
  for (const auto& entry : entries) {
    variant_keys_list.emplace_back(entry->variant_key().Utf8());
  }
  std::string accept_langs;
  for (const auto& language : languages) {
    if (!accept_langs.empty())
      accept_langs += ",";
    accept_langs += language.Utf8();
  }
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(net::HttpRequestHeaders::kAccept, accept_header);

  WebPackageRequestMatcher matcher(request_headers, accept_langs);
  const auto variant_keys_list_it =
      matcher.FindBestMatchingVariantKey(variants, variant_keys_list);
  if (variant_keys_list_it == variant_keys_list.end())
    return nullptr;
  return (entries.begin() + (variant_keys_list_it - variant_keys_list.begin()))
      ->get();
}

}  // namespace blink
