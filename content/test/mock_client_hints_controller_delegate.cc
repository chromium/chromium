// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_client_hints_controller_delegate.h"

#include "content/public/common/origin_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace content {

namespace {

bool PersistClientHintsHelper(const GURL& url,
                              const blink::EnabledClientHints& client_hints,
                              ClientHintsContainer* container) {
  DCHECK(container);
  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    return false;
  }
  const url::Origin origin = url::Origin::Create(url);
  (*container)[origin] = client_hints;
  return true;
}

void GetAllowedClientHintsFromSourceHelper(
    const url::Origin& origin,
    const ClientHintsContainer& container,
    blink::EnabledClientHints* client_hints) {
  const auto& it = container.find(origin);
  DCHECK(client_hints);
  if (it != container.end()) {
    *client_hints = it->second;
  }
}

}  // namespace

MockClientHintsControllerDelegate::MockClientHintsControllerDelegate(
    const blink::UserAgentMetadata& metadata)
    : metadata_(metadata) {}

MockClientHintsControllerDelegate::~MockClientHintsControllerDelegate() =
    default;

network::NetworkQualityTracker*
MockClientHintsControllerDelegate::GetNetworkQualityTracker() {
  return &network_quality_tracker_;
}

bool MockClientHintsControllerDelegate::IsJavaScriptAllowed(
    const GURL& url,
    content::RenderFrameHost* parent_rfh) {
  return true;
}

bool MockClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  return false;
}

blink::UserAgentMetadata
MockClientHintsControllerDelegate::GetUserAgentMetadata() {
  return metadata_;
}

void MockClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    content::RenderFrameHost* parent_rfh,
    const std::vector<::network::mojom::WebClientHintsType>& client_hints) {
  blink::EnabledClientHints enabled_client_hints;
  for (const auto& type : client_hints) {
    enabled_client_hints.SetIsEnabled(type, true);
  }

  PersistClientHintsHelper(primary_origin.GetURL(), enabled_client_hints,
                           &client_hints_map_);
}

// Get which client hints opt-ins were persisted on current origin.
void MockClientHintsControllerDelegate::GetAllowedClientHintsFromSource(
    const url::Origin& origin,
    blink::EnabledClientHints* client_hints) {
  GetAllowedClientHintsFromSourceHelper(origin, client_hints_map_,
                                        client_hints);
  for (auto hint : additional_hints_)
    client_hints->SetIsEnabled(hint, true);
}

void MockClientHintsControllerDelegate::ResetForTesting() {
  client_hints_map_.clear();
  additional_hints_.clear();
}

void MockClientHintsControllerDelegate::SetAdditionalClientHints(
    const std::vector<network::mojom::WebClientHintsType>& hints) {
  additional_hints_ = hints;
}

void MockClientHintsControllerDelegate::ClearAdditionalClientHints() {
  additional_hints_.clear();
}

void MockClientHintsControllerDelegate::SetMostRecentMainFrameViewportSize(
    const gfx::Size& viewport_size) {
  viewport_size_ = viewport_size;
}

gfx::Size
MockClientHintsControllerDelegate::GetMostRecentMainFrameViewportSize() {
  return viewport_size_;
}

}  // end namespace content
