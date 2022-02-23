// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_client_hints_controller_delegate.h"

#include "content/public/common/origin_util.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace content {

MockClientHintsControllerDelegate::MockClientHintsControllerDelegate(
    const blink::UserAgentMetadata& metadata)
    : metadata_(metadata) {}

MockClientHintsControllerDelegate::~MockClientHintsControllerDelegate() =
    default;

network::NetworkQualityTracker*
MockClientHintsControllerDelegate::GetNetworkQualityTracker() {
  return nullptr;
}

bool MockClientHintsControllerDelegate::IsJavaScriptAllowed(const GURL& url) {
  return true;
}

bool MockClientHintsControllerDelegate::AreThirdPartyCookiesBlocked(
    const GURL& url) {
  return false;
}

blink::UserAgentMetadata
MockClientHintsControllerDelegate::GetUserAgentMetadata() {
  return metadata_;
}

void MockClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
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
    const GURL& url,
    blink::EnabledClientHints* client_hints) {
  GetAllowedClientHintsFromSourceHelper(url, client_hints_map_, client_hints);
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

}  // end namespace content
