// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher_base.h"

#include <tuple>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

bool ShouldCollapseResourceType(flat_rule::ElementType type) {
  // TODO(crbug.com/848842): Add support for other element types like
  // OBJECT.
  return type == flat_rule::ElementType_IMAGE ||
         type == flat_rule::ElementType_SUBDOCUMENT;
}

bool IsUpgradeableUrl(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kFtpScheme);
}

// Upgrades the url's scheme to HTTPS.
GURL GetUpgradedUrl(const GURL& url) {
  DCHECK(IsUpgradeableUrl(url));
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  return url.ReplaceComponents(replacements);
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
      remove_query_params.push_back(CreateString<base::StringPiece>(*str));
  }

  // We don't use a map from keys to vector of values to ensure the relative
  // order of different params specified by the extension is respected. We use a
  // std::list to support fast removal from middle of the list. Note that the
  // key value pairs should already be escaped.
  struct QueryReplace {
    base::StringPiece key;
    base::StringPiece value;
    bool replace_only = false;
  };
  std::list<QueryReplace> add_or_replace_query_params;

  if (!IsEmpty(transform.add_or_replace_query_params())) {
    for (const flat::QueryKeyValue* query_pair :
         *transform.add_or_replace_query_params()) {
      DCHECK(query_pair->key());
      DCHECK(query_pair->value());
      add_or_replace_query_params.emplace_back(
          QueryReplace{CreateString<base::StringPiece>(*query_pair->key()),
                       CreateString<base::StringPiece>(*query_pair->value()),
                       query_pair->replace_only()});
    }
  }

  std::vector<std::string> query_parts;

  auto create_query_part = [](base::StringPiece key,
                              base::StringPiece value) -> std::string {
    return base::StrCat({key, "=", value});
  };

  bool query_changed = false;
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    const base::StringPiece key = it.GetKey();
    // Remove query param.
    if (std::binary_search(remove_query_params.begin(),
                           remove_query_params.end(), key)) {
      query_changed = true;
      continue;
    }

    auto replace_iterator = base::ranges::find(add_or_replace_query_params, key,
                                               &QueryReplace::key);

    // Nothing to do.
    if (replace_iterator == add_or_replace_query_params.end()) {
      query_parts.push_back(create_query_part(key, it.GetValue()));
      continue;
    }

    // Replace query param.
    query_changed = true;
    query_parts.push_back(create_query_part(key, replace_iterator->value));
    add_or_replace_query_params.erase(replace_iterator);
  }

  // Append any remaining query params.
  for (const auto& params : add_or_replace_query_params) {
    if (!params.replace_only) {
      query_parts.push_back(create_query_part(params.key, params.value));
      query_changed = true;
    }
  }

  if (!query_changed)
    return false;

  *modified_query = base::JoinString(query_parts, "&");
  return true;
}

GURL GetTransformedURL(const RequestParams& params,
                       const flat::UrlTransform& transform) {
  GURL::Replacements replacements;

  if (transform.scheme())
    replacements.SetSchemeStr(
        CreateString<base::StringPiece>(*transform.scheme()));

  if (transform.host())
    replacements.SetHostStr(CreateString<base::StringPiece>(*transform.host()));

  DCHECK(!(transform.clear_port() && transform.port()));
  if (transform.clear_port())
    replacements.ClearPort();
  else if (transform.port())
    replacements.SetPortStr(CreateString<base::StringPiece>(*transform.port()));

  DCHECK(!(transform.clear_path() && transform.path()));
  if (transform.clear_path())
    replacements.ClearPath();
  else if (transform.path())
    replacements.SetPathStr(CreateString<base::StringPiece>(*transform.path()));

  // |query| is defined outside the if conditions since url::Replacements does
  // not own the strings it uses.
  std::string query;
  if (transform.clear_query()) {
    replacements.ClearQuery();
  } else if (transform.query()) {
    replacements.SetQueryStr(
        CreateString<base::StringPiece>(*transform.query()));
  } else if (GetModifiedQuery(*params.url, transform, &query)) {
    replacements.SetQueryStr(query);
  }

  DCHECK(!(transform.clear_fragment() && transform.fragment()));
  if (transform.clear_fragment())
    replacements.ClearRef();
  else if (transform.fragment())
    replacements.SetRefStr(
        CreateString<base::StringPiece>(*transform.fragment()));

  if (transform.password())
    replacements.SetPasswordStr(
        CreateString<base::StringPiece>(*transform.password()));

  if (transform.username())
    replacements.SetUsernameStr(
        CreateString<base::StringPiece>(*transform.username()));

  return params.url->ReplaceComponents(replacements);
}

}  // namespace

RulesetMatcherBase::RulesetMatcherBase(const ExtensionId& extension_id,
                                       RulesetID ruleset_id)
    : extension_id_(extension_id), ruleset_id_(ruleset_id) {}
RulesetMatcherBase::~RulesetMatcherBase() = default;

absl::optional<RequestAction> RulesetMatcherBase::GetBeforeRequestAction(
    const RequestParams& params) const {
  absl::optional<RequestAction> action =
      GetBeforeRequestActionIgnoringAncestors(params);
  absl::optional<RequestAction> parent_action =
      GetAllowlistedFrameAction(params.parent_routing_id);

  return GetMaxPriorityAction(std::move(action), std::move(parent_action));
}

void RulesetMatcherBase::OnRenderFrameCreated(content::RenderFrameHost* host) {
  DCHECK(host);
  content::RenderFrameHost* parent = host->GetParentOrOuterDocument();
  if (!parent)
    return;

  // Some frames like srcdoc frames inherit URLLoaderFactories from their
  // parents and can make network requests before a corresponding navigation
  // commit for the frame is received in the browser (via DidFinishNavigation).
  // Hence if the parent frame is allowlisted, we allow list the current frame
  // as well in OnRenderFrameCreated.
  absl::optional<RequestAction> parent_action =
      GetAllowlistedFrameAction(parent->GetGlobalId());
  if (!parent_action)
    return;

  bool inserted = false;
  std::tie(std::ignore, inserted) = allowlisted_frames_.insert(
      std::make_pair(host->GetGlobalId(), std::move(*parent_action)));
  DCHECK(inserted);
}

void RulesetMatcherBase::OnRenderFrameDeleted(content::RenderFrameHost* host) {
  DCHECK(host);
  allowlisted_frames_.erase(host->GetGlobalId());
}

void RulesetMatcherBase::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* host = navigation_handle->GetRenderFrameHost();

  // Note: we only start tracking frames on navigation, since a document only
  // issues network requests after the corresponding navigation is committed.
  // Hence we need not listen to OnRenderFrameCreated.
  DCHECK(host);

  RequestParams params(host, navigation_handle->IsPost());

  // Find the highest priority allowAllRequests action corresponding to this
  // frame.
  absl::optional<RequestAction> parent_action =
      GetAllowlistedFrameAction(params.parent_routing_id);
  absl::optional<RequestAction> frame_action =
      GetAllowAllRequestsAction(params);
  absl::optional<RequestAction> action =
      GetMaxPriorityAction(std::move(parent_action), std::move(frame_action));

  content::GlobalRenderFrameHostId frame_id = host->GetGlobalId();
  allowlisted_frames_.erase(frame_id);

  if (action)
    allowlisted_frames_.insert(std::make_pair(frame_id, std::move(*action)));
}

absl::optional<RequestAction>
RulesetMatcherBase::GetAllowlistedFrameActionForTesting(
    content::RenderFrameHost* host) const {
  DCHECK(host);
  return GetAllowlistedFrameAction(host->GetGlobalId());
}

RequestAction RulesetMatcherBase::CreateBlockOrCollapseRequestAction(
    const RequestParams& params,
    const flat_rule::UrlRule& rule) const {
  return CreateRequestAction(ShouldCollapseResourceType(params.element_type)
                                 ? RequestAction::Type::COLLAPSE
                                 : RequestAction::Type::BLOCK,
                             rule);
}

RequestAction RulesetMatcherBase::CreateAllowAction(
    const RequestParams& params,
    const flat_rule::UrlRule& rule) const {
  return CreateRequestAction(RequestAction::Type::ALLOW, rule);
}

RequestAction RulesetMatcherBase::CreateAllowAllRequestsAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule) const {
  return CreateRequestAction(RequestAction::Type::ALLOW_ALL_REQUESTS, rule);
}

absl::optional<RequestAction> RulesetMatcherBase::CreateUpgradeAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule) const {
  if (!IsUpgradeableUrl(*params.url)) {
    // TODO(crbug.com/1033780): this results in counterintuitive behavior.
    return absl::nullopt;
  }
  RequestAction upgrade_action =
      CreateRequestAction(RequestAction::Type::UPGRADE, rule);
  upgrade_action.redirect_url = GetUpgradedUrl(*params.url);
  return upgrade_action;
}

absl::optional<RequestAction>
RulesetMatcherBase::CreateRedirectActionFromMetadata(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule,
    const ExtensionMetadataList& metadata_list) const {
  // Find the UrlRuleMetadata corresponding to |rule|. Since |metadata_list| is
  // sorted by rule id, use LookupByKey which binary searches for fast lookup.
  const flat::UrlRuleMetadata* metadata = metadata_list.LookupByKey(rule.id());

  // There must be a UrlRuleMetadata object corresponding to the |rule|.
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule.id());
  DCHECK(metadata->redirect_url() || metadata->transform());

  GURL redirect_url;
  if (metadata->redirect_url())
    redirect_url =
        GURL(CreateString<base::StringPiece>(*metadata->redirect_url()));
  else
    redirect_url = GetTransformedURL(params, *metadata->transform());

  // Sanity check that we don't redirect to a javascript url. Specifying
  // redirect to a javascript url and specifying javascript as a transform
  // scheme is prohibited. In addition extensions can't intercept requests to
  // javascript urls. Hence we should never end up with a javascript url here.
  DCHECK(!redirect_url.SchemeIs(url::kJavaScriptScheme));

  return CreateRedirectAction(params, rule, std::move(redirect_url));
}

absl::optional<RequestAction> RulesetMatcherBase::CreateRedirectAction(
    const RequestParams& params,
    const url_pattern_index::flat::UrlRule& rule,
    GURL redirect_url) const {
  // Redirecting WebSocket handshake request is prohibited.
  // TODO(crbug.com/1033780): this results in counterintuitive behavior.
  if (params.element_type == flat_rule::ElementType_WEBSOCKET)
    return absl::nullopt;

  // Prevent a redirect loop where a URL continuously redirects to itself.
  if (!redirect_url.is_valid() || *params.url == redirect_url)
    return absl::nullopt;

  RequestAction redirect_action =
      CreateRequestAction(RequestAction::Type::REDIRECT, rule);
  redirect_action.redirect_url = std::move(redirect_url);
  return redirect_action;
}

std::vector<RequestAction>
RulesetMatcherBase::GetModifyHeadersActionsFromMetadata(
    const RequestParams& params,
    const std::vector<const url_pattern_index::flat::UrlRule*>& rules,
    const ExtensionMetadataList& metadata_list) const {
  using FlatHeaderList = flatbuffers::Vector<flatbuffers::Offset<
      extensions::declarative_net_request::flat::ModifyHeaderInfo>>;

  // Helper method to convert a list of headers from a rule's metadata to a list
  // of RequestAction::HeaderInfo.
  auto get_headers_for_action = [](const FlatHeaderList& headers_for_rule) {
    std::vector<RequestAction::HeaderInfo> headers_for_action;
    for (const auto* flat_header_info : headers_for_rule)
      headers_for_action.emplace_back(*flat_header_info);

    return headers_for_action;
  };

  std::vector<RequestAction> actions;
  for (const auto* rule : rules) {
    const flat::UrlRuleMetadata* metadata =
        metadata_list.LookupByKey(rule->id());

    DCHECK(metadata);
    DCHECK_EQ(metadata->id(), rule->id());

    RequestAction action =
        CreateRequestAction(RequestAction::Type::MODIFY_HEADERS, *rule);
    action.request_headers_to_modify =
        get_headers_for_action(*metadata->request_headers());
    action.response_headers_to_modify =
        get_headers_for_action(*metadata->response_headers());

    actions.push_back(std::move(action));
  }

  return actions;
}

RequestAction RulesetMatcherBase::CreateRequestAction(
    RequestAction::Type type,
    const flat_rule::UrlRule& rule) const {
  return RequestAction(type, rule.id(), rule.priority(), ruleset_id(),
                       extension_id());
}

absl::optional<RequestAction> RulesetMatcherBase::GetAllowlistedFrameAction(
    content::GlobalRenderFrameHostId frame_id) const {
  auto it = allowlisted_frames_.find(frame_id);
  if (it == allowlisted_frames_.end())
    return absl::nullopt;

  return it->second.Clone();
}

}  // namespace declarative_net_request
}  // namespace extensions
