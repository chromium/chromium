// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_client_hints_controller_delegate.h"

#include "base/feature_list.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace headless {

namespace {

// These hints need a network quality tracker, which headless does not support,
// so never permit them.
constexpr network::mojom::WebClientHintsType kNotSupportedClientHints[] = {
    network::mojom::WebClientHintsType::kRtt_DEPRECATED,
    network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
    network::mojom::WebClientHintsType::kEct_DEPRECATED,
};

}  // namespace

HeadlessClientHintsControllerDelegate::HeadlessClientHintsControllerDelegate() =
    default;
HeadlessClientHintsControllerDelegate::
    ~HeadlessClientHintsControllerDelegate() = default;

// Always returns nullptr.
network::NetworkQualityTracker*
HeadlessClientHintsControllerDelegate::GetNetworkQualityTracker() {
  return nullptr;
}

// Network related hints are disabled unconditionally due to not having a
// NetworkQualityTracker.
void HeadlessClientHintsControllerDelegate::GetAllowedClientHintsFromSource(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) {
  DCHECK(client_hints);
  const auto& it = persist_hints_.find(origin);
  if (it != persist_hints_.end()) {
    *client_hints = it->second;
  }
  for (auto hint : additional_hints_) {
    client_hints->SetIsEnabled(hint, true);
  }
  for (auto& hint : kNotSupportedClientHints) {
    client_hints->SetIsEnabled(hint, false);
  }
}

// TODO(crbug.com/40257952): Currently always returns true.
bool HeadlessClientHintsControllerDelegate::IsJavaScriptAllowed(
    const GURL& url,
    content::RenderFrameHost* parent_rfh) {
  return true;
}

// TODO(crbug.com/40257952): Currently always returns false.
bool HeadlessClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  return false;
}

blink::UserAgentMetadata
HeadlessClientHintsControllerDelegate::GetUserAgentMetadata() {
  return HeadlessBrowser::GetUserAgentMetadata();
}

void HeadlessClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  const GURL primary_url = primary_origin.GetURL();
  if (!primary_url.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(primary_url)) {
    return;
  }

  if (!IsJavaScriptAllowed(primary_url, parent_rfh)) {
    return;
  }

  blink::EnabledClientHints persist_hints;
  for (const auto& type : client_hints) {
    persist_hints.SetIsEnabled(type, true);
  }
  persist_hints_[primary_origin] = persist_hints;
}

void HeadlessClientHintsControllerDelegate::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& client_hints) {
  additional_hints_ = client_hints;
}

void HeadlessClientHintsControllerDelegate::ClearAdditionalClientHints() {
  additional_hints_.clear();
}

void HeadlessClientHintsControllerDelegate::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Size
HeadlessClientHintsControllerDelegate::GetMostRecentMainFrameViewportSize() {
  return viewport_size_;
}

}  // namespace headless
