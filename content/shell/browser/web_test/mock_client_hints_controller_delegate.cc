// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/mock_client_hints_controller_delegate.h"
#include "content/public/common/origin_util.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace content {

MockClientHintsControllerDelegate::MockClientHintsControllerDelegate() {}
MockClientHintsControllerDelegate::~MockClientHintsControllerDelegate() {}

void MockClientHintsControllerDelegate::Bind(
    mojo::PendingReceiver<client_hints::mojom::ClientHints> receiver) {
  receivers_.Add(this, std::move(receiver));
}

network::NetworkQualityTracker*
MockClientHintsControllerDelegate::GetNetworkQualityTracker() {
  return nullptr;
}

bool MockClientHintsControllerDelegate::IsJavaScriptAllowed(const GURL& url) {
  return true;
}

std::string MockClientHintsControllerDelegate::GetAcceptLanguageString() {
  return content::GetShellLanguage();
}

blink::UserAgentMetadata
MockClientHintsControllerDelegate::GetUserAgentMetadata() {
  return content::GetShellUserAgentMetadata();
}

void MockClientHintsControllerDelegate::PersistClientHints(
    const url::Origin& primary_origin,
    const std::vector<::blink::mojom::WebClientHintsType>& client_hints,
    base::TimeDelta expiration_duration) {
  blink::WebEnabledClientHints web_client_hints;
  for (const auto& type : client_hints) {
    web_client_hints.SetIsEnabled(type, true);
  }

  PersistClientHintsHelper(primary_origin.GetURL(), web_client_hints,
                           expiration_duration, &client_hints_map_);
}

// Get which client hints opt-ins were persisted on current origin.
void MockClientHintsControllerDelegate::GetAllowedClientHintsFromSource(
    const GURL& url,
    blink::WebEnabledClientHints* client_hints) {
  GetAllowedClientHintsFromSourceHelper(url, client_hints_map_, client_hints);
}

void MockClientHintsControllerDelegate::ResetForTesting() {
  client_hints_map_.clear();
}

}  // end namespace content
