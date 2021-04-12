// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"

#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/engine/url_request_rewrite_type_converters.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

bool IsValidUrlHost(base::StringPiece host) {
  return GURL(base::StrCat({url::kHttpScheme, "://", host})).is_valid();
}

bool ValidateAddHeaders(
    const fuchsia::web::UrlRequestRewriteAddHeaders& add_headers) {
  if (!add_headers.has_headers())
    return false;
  for (const auto& header : add_headers.headers()) {
    base::StringPiece header_name = cr_fuchsia::BytesAsString(header.name);
    base::StringPiece header_value = cr_fuchsia::BytesAsString(header.value);
    if (!net::HttpUtil::IsValidHeaderName(header_name) ||
        !net::HttpUtil::IsValidHeaderValue(header_value))
      return false;
  }
  return true;
}

bool ValidateRemoveHeader(
    const fuchsia::web::UrlRequestRewriteRemoveHeader& remove_header) {
  if (!remove_header.has_header_name())
    return false;
  if (!net::HttpUtil::IsValidHeaderName(
          cr_fuchsia::BytesAsString(remove_header.header_name())))
    return false;
  return true;
}

bool ValidateSubstituteQueryPattern(
    const fuchsia::web::UrlRequestRewriteSubstituteQueryPattern&
        substitute_query_pattern) {
  if (!substitute_query_pattern.has_pattern() ||
      !substitute_query_pattern.has_substitution())
    return false;
  return true;
}

bool ValidateReplaceUrl(
    const fuchsia::web::UrlRequestRewriteReplaceUrl& replace_url) {
  if (!replace_url.has_url_ends_with() || !replace_url.has_new_url())
    return false;
  if (!GURL("http://site.com/" + replace_url.url_ends_with()).is_valid())
    return false;
  if (!GURL(replace_url.new_url()).is_valid())
    return false;
  return true;
}

bool ValidateAppendToQuery(
    const fuchsia::web::UrlRequestRewriteAppendToQuery& append_to_query) {
  if (!append_to_query.has_query())
    return false;
  return true;
}

bool ValidateRewrite(const fuchsia::web::UrlRequestRewrite& rewrite) {
  switch (rewrite.Which()) {
    case fuchsia::web::UrlRequestRewrite::Tag::kAddHeaders:
      return ValidateAddHeaders(rewrite.add_headers());
    case fuchsia::web::UrlRequestRewrite::Tag::kRemoveHeader:
      return ValidateRemoveHeader(rewrite.remove_header());
    case fuchsia::web::UrlRequestRewrite::Tag::kSubstituteQueryPattern:
      return ValidateSubstituteQueryPattern(rewrite.substitute_query_pattern());
    case fuchsia::web::UrlRequestRewrite::Tag::kReplaceUrl:
      return ValidateReplaceUrl(rewrite.replace_url());
    case fuchsia::web::UrlRequestRewrite::Tag::kAppendToQuery:
      return ValidateAppendToQuery(rewrite.append_to_query());
    default:
      // This is to prevent build breakage when adding new rewrites to the FIDL
      // definition. This can also happen if the client sends an empty rewrite,
      // which is invalid.
      return false;
  }
}

bool ValidateRules(
    const std::vector<fuchsia::web::UrlRequestRewriteRule>& rules) {
  for (const auto& rule : rules) {
    if (rule.has_hosts_filter()) {
      if (rule.hosts_filter().empty())
        return false;

      const base::StringPiece kWildcard("*.");
      for (const base::StringPiece host : rule.hosts_filter()) {
        if (base::StartsWith(host, kWildcard, base::CompareCase::SENSITIVE)) {
          if (!IsValidUrlHost(host.substr(2)))
            return false;
        } else {
          if (!IsValidUrlHost(host))
            return false;
        }
      }
    }
    if (rule.has_schemes_filter() && rule.schemes_filter().empty())
      return false;

    if (rule.has_rewrites()) {
      if (rule.has_action())
        return false;

      for (const auto& rewrite : rule.rewrites()) {
        if (!ValidateRewrite(rewrite))
          return false;
      }
    } else if (!rule.has_action()) {
      // No rewrites, no action = no point!
      return false;
    }
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<UrlRequestRewriteRulesManager>
UrlRequestRewriteRulesManager::CreateForTesting() {
  return base::WrapUnique(new UrlRequestRewriteRulesManager());
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

UrlRequestRewriteRulesManager::~UrlRequestRewriteRulesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

zx_status_t UrlRequestRewriteRulesManager::OnRulesUpdated(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    fuchsia::web::Frame::SetUrlRequestRewriteRulesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateRules(rules)) {
    cached_rules_ = nullptr;
    return ZX_ERR_INVALID_ARGS;
  }

  cached_rules_ =
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          mojo::ConvertTo<std::vector<mojom::UrlRequestRulePtr>>(
              std::move(rules)));

  // Send the updated rules to the receivers.
  for (const auto& receiver_pair : active_remotes_) {
    receiver_pair.second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }

  // TODO(crbug.com/976975): Only call the callback when there are pending
  // throttles.
  std::move(callback)();
  return ZX_OK;
}

scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>&
UrlRequestRewriteRulesManager::GetCachedRules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_rules_;
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager() = default;

void UrlRequestRewriteRulesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Register the frame rules receiver.
  mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver> rules_receiver;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &rules_receiver);
  auto iter = active_remotes_.emplace(
      render_frame_host->GetGlobalFrameRoutingId(), std::move(rules_receiver));
  DCHECK(iter.second);

  if (cached_rules_) {
    // Send an initial set of rules.
    iter.first->second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }
}

void UrlRequestRewriteRulesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t removed =
      active_remotes_.erase(render_frame_host->GetGlobalFrameRoutingId());
  DCHECK_EQ(removed, 1u);
}
