// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/test/mock_client_hints_utils.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "url/origin.h"

namespace content {

class MockClientHintsControllerDelegate : public ClientHintsControllerDelegate {
 public:
  explicit MockClientHintsControllerDelegate(
      const blink::UserAgentMetadata& metadata);
  ~MockClientHintsControllerDelegate() override;
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  // Get which client hints opt-ins were persisted on current origin.
  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  bool UserAgentClientHintEnabled() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<::network::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override;

  void ResetForTesting() override;

  void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>&) override;

  void ClearAdditionalClientHints() override;

 private:
  const blink::UserAgentMetadata metadata_;
  ClientHintsContainer client_hints_map_;
  std::vector<network::mojom::WebClientHintsType> additional_hints_;

  DISALLOW_COPY_AND_ASSIGN(MockClientHintsControllerDelegate);
};
}  // end namespace content

#endif  // CONTENT_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
