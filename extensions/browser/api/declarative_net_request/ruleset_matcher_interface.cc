// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher_interface.h"

#include "base/strings/strcat.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

constexpr const char kSetCookieResponseHeader[] = "set-cookie";

bool ShouldCollapseResourceType(flat_rule::ElementType type) {
  // TODO(crbug.com/848842): Add support for other element types like
  // OBJECT.
  return type == flat_rule::ElementType_IMAGE ||
         type == flat_rule::ElementType_SUBDOCUMENT;
}

// Upgrades the url's scheme to HTTPS.
GURL GetUpgradedUrl(const GURL& url) {
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  return url.ReplaceComponents(replacements);
}

base::StringPiece CreateStringPiece(const ::flatbuffers::String& str) {
  return base::StringPiece(str.c_str(), str.size());
}

// Returns true if the given |vec| is nullptr or empty.
template <typename T>
bool IsEmpty(const flatbuffers::Vector<T>* vec) {
  return !vec || vec->size() == 0;
}

// Performs any required query transformations on the |url|. Returns true if the
// query should be modified and populates |modified_query|.
bool GetModifiedQuery(const GURL& url,
                      const flat::UrlTransform& transform,
                      std::string* modified_query) {
  DCHECK(modified_query);

  // |remove_query_params| should always be sorted.
  DCHECK(
      IsEmpty(transform.remove_query_params()) ||
      std::is_sorted(transform.remove_query_params()->begin(),
                     transform.remove_query_params()->end(),
                     [](const flatbuffers::String* x1,
                        const flatbuffers::String* x2) { return *x1 < *x2; }));

  // Return early if there's nothing to modify.
  if (IsEmpty(transform.remove_query_params()) &&
      IsEmpty(transform.add_or_replace_query_params())) {
    return false;
  }

  std::vector<base::StringPiece> remove_query_params;
  if (!IsEmpty(transform.remove_query_params())) {
    remove_query_params.reserve(transform.remove_query_params()->size());
    for (const ::flatbuffers::String* str : *transform.remove_query_params())
      remove_query_params.push_back(CreateStringPiece(*str));
  }

  // We don't use a map from keys to vector of values to ensure the relative
  // order of different params specified by the extension is respected. We use a
  // std::list to support fast removal from middle of the list. Note that the
  // key value pairs should already be escaped.
  std::list<std::pair<base::StringPiece, base::StringPiece>>
      add_or_replace_query_params;
  if (!IsEmpty(transform.add_or_replace_query_params())) {
    for (const flat::QueryKeyValue* query_pair :
         *transform.add_or_replace_query_params()) {
      DCHECK(query_pair->key());
      DCHECK(query_pair->value());
      add_or_replace_query_params.emplace_back(
          CreateStringPiece(*query_pair->key()),
          CreateStringPiece(*query_pair->value()));
    }
  }

  std::vector<std::string> query_parts;

  auto create_query_part = [](base::StringPiece key,
                              base::StringPiece value) -> std::string {
    return base::StrCat({key, "=", value});
  };

  bool query_changed = false;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    std::string key = it.GetKey();
    // Remove query param.
    if (std::binary_search(remove_query_params.begin(),
                           remove_query_params.end(), key)) {
      query_changed = true;
      continue;
    }

    auto replace_iterator = std::find_if(
        add_or_replace_query_params.begin(), add_or_replace_query_params.end(),
        [&key](const std::pair<base::StringPiece, base::StringPiece>& param) {
          return param.first == key;
        });

    // Nothing to do.
    if (replace_iterator == add_or_replace_query_params.end()) {
      query_parts.push_back(create_query_part(key, it.GetValue()));
      continue;
    }

    // Replace query param.
    query_changed = true;
    query_parts.push_back(create_query_part(key, replace_iterator->second));
    add_or_replace_query_params.erase(replace_iterator);
  }

  // Append any remaining query params.
  for (const auto& params : add_or_replace_query_params)
    query_parts.push_back(create_query_part(params.first, params.second));

  query_changed |= !add_or_replace_query_params.empty();

  if (!query_changed)
    return false;

  *modified_query = base::JoinString(query_parts, "&");
  return true;
}

GURL GetTransformedURL(const RequestParams& params,
                       const flat::UrlTransform& transform) {
  GURL::Replacements replacements;

  if (transform.scheme())
    replacements.SetSchemeStr(CreateStringPiece(*transform.scheme()));

  if (transform.host())
    replacements.SetHostStr(CreateStringPiece(*transform.host()));

  DCHECK(!(transform.clear_port() && transform.port()));
  if (transform.clear_port())
    replacements.ClearPort();
  else if (transform.port())
    replacements.SetPortStr(CreateStringPiece(*transform.port()));

  DCHECK(!(transform.clear_path() && transform.path()));
  if (transform.clear_path())
    replacements.ClearPath();
  else if (transform.path())
    replacements.SetPathStr(CreateStringPiece(*transform.path()));

  // |query| is defined outside the if conditions since url::Replacements does
  // not own the strings it uses.
  std::string query;
  if (transform.clear_query()) {
    replacements.ClearQuery();
  } else if (transform.query()) {
    replacements.SetQueryStr(CreateStringPiece(*transform.query()));
  } else if (GetModifiedQuery(*params.url, transform, &query)) {
    replacements.SetQueryStr(query);
  }

  DCHECK(!(transform.clear_fragment() && transform.fragment()));
  if (transform.clear_fragment())
    replacements.ClearRef();
  else if (transform.fragment())
    replacements.SetRefStr(CreateStringPiece(*transform.fragment()));

  if (transform.password())
    replacements.SetPasswordStr(CreateStringPiece(*transform.password()));

  if (transform.username())
    replacements.SetUsernameStr(CreateStringPiece(*transform.username()));

  return params.url->ReplaceComponents(replacements);
}

}  // namespace

RulesetMatcherInterface::RulesetMatcherInterface(
    const ExtensionId& extension_id,
    api::declarative_net_request::SourceType source_type)
    : extension_id_(extension_id), source_type_(source_type) {}
RulesetMatcherInterface::~RulesetMatcherInterface() = default;

// static
bool RulesetMatcherInterface::IsUpgradeableRequest(
    const RequestParams& params) {
  return params.url->SchemeIs(url::kHttpScheme) ||
         params.url->SchemeIs(url::kFtpScheme);
}

RequestAction RulesetMatcherInterface::CreateBlockOrCollapseRequestAction(
    const RequestParams& params,
    const flat_rule::UrlRule& rule) const {
  return ShouldCollapseResourceType(params.element_type)
             ? RequestAction(RequestAction::Type::COLLAPSE, rule.id(),
                             rule.priority(), source_type(), extension_id())
             : RequestAction(RequestAction::Type::BLOCK, rule.id(),
                             rule.priority(), source_type(), extension_id());
}

RequestAction RulesetMatcherInterface::CreateAllowAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule) const {
  return RequestAction(RequestAction::Type::ALLOW, rule.id(), rule.priority(),
                       source_type(), extension_id());
}

RequestAction RulesetMatcherInterface::CreateUpgradeAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule) const {
  DCHECK(IsUpgradeableRequest(params));

  RequestAction upgrade_action(RequestAction::Type::REDIRECT, rule.id(),
                               rule.priority(), source_type(), extension_id());
  upgrade_action.redirect_url = GetUpgradedUrl(*params.url);
  return upgrade_action;
}

base::Optional<RequestAction> RulesetMatcherInterface::CreateRedirectAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule,
    const ExtensionMetadataList& metadata_list) const {
  DCHECK_NE(flat_rule::ElementType_WEBSOCKET, params.element_type);

  // Find the UrlRuleMetadata corresponding to |rule|. Since |metadata_list| is
  // sorted by rule id, use LookupByKey which binary searches for fast lookup.
  const flat::UrlRuleMetadata* metadata = metadata_list.LookupByKey(rule.id());

  // There must be a UrlRuleMetadata object corresponding to each redirect rule.
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule.id());
  DCHECK(metadata->redirect_url() || metadata->transform());

  GURL redirect_url;
  if (metadata->redirect_url())
    redirect_url = GURL(CreateStringPiece(*metadata->redirect_url()));
  else
    redirect_url = GetTransformedURL(params, *metadata->transform());

  // Sanity check that we don't redirect to a javascript url. Specifying
  // redirect to a javascript url and specifying javascript as a transform
  // scheme is prohibited. In addition extensions can't intercept requests to
  // javascript urls. Hence we should never end up with a javascript url here.
  DCHECK(!redirect_url.SchemeIs(url::kJavaScriptScheme));

  // Prevent a redirect loop where a URL continuously redirects to itself.
  if (!redirect_url.is_valid() || *params.url == redirect_url)
    return base::nullopt;

  RequestAction redirect_action(RequestAction::Type::REDIRECT, rule.id(),
                                rule.priority(), source_type(), extension_id());
  redirect_action.redirect_url = std::move(redirect_url);
  return redirect_action;
}

RequestAction RulesetMatcherInterface::GetRemoveHeadersActionForMask(
    const url_pattern_index::flat::UrlRule& rule,
    uint8_t mask) const {
  DCHECK(mask);
  RequestAction action(RequestAction::Type::REMOVE_HEADERS, rule.id(),
                       rule.priority(), source_type(), extension_id());

  for (int header = 0; header <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++header) {
    switch (header) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        if (mask & flat::RemoveHeaderType_cookie) {
          action.request_headers_to_remove.push_back(
              net::HttpRequestHeaders::kCookie);
        }
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        if (mask & flat::RemoveHeaderType_referer) {
          action.request_headers_to_remove.push_back(
              net::HttpRequestHeaders::kReferer);
        }
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        if (mask & flat::RemoveHeaderType_set_cookie)
          action.response_headers_to_remove.push_back(kSetCookieResponseHeader);
        break;
    }
  }

  return action;
}

}  // namespace declarative_net_request
}  // namespace extensions
