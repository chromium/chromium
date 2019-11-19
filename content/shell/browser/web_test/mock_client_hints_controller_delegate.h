// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/shell/utility/mock_client_hints_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "url/origin.h"

namespace content {

class MockClientHintsControllerDelegate
    : public content::ClientHintsControllerDelegate {
 public:
  MockClientHintsControllerDelegate();
  ~MockClientHintsControllerDelegate() override;
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  // Get which client hints opt-ins were persisted on current origin.
  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  std::string GetAcceptLanguageString() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;
  // mojom::ClientHints implementation.
  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<::blink::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override;

  void Bind(mojo::PendingReceiver<client_hints::mojom::ClientHints> receiver)
      override;

  void ResetForTesting() override;

 private:
  ClientHintsContainer client_hints_map_;
  mojo::ReceiverSet<client_hints::mojom::ClientHints> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MockClientHintsControllerDelegate);
};
}  // end namespace content
#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
