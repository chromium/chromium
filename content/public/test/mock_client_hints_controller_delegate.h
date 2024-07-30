// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include "content/public/browser/client_hints_controller_delegate.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/origin.h"

namespace content {

using ClientHintsContainer =
    std::map<const url::Origin, blink::EnabledClientHints>;

class MockClientHintsControllerDelegate : public ClientHintsControllerDelegate {
 public:
  explicit MockClientHintsControllerDelegate(
      const blink::UserAgentMetadata& metadata);

  MockClientHintsControllerDelegate(const MockClientHintsControllerDelegate&) =
      delete;
  MockClientHintsControllerDelegate& operator=(
      const MockClientHintsControllerDelegate&) = delete;

  ~MockClientHintsControllerDelegate() override;
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  // Get which client hints opt-ins were persisted on current origin.
  void GetAllowedClientHintsFromSource(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url,
                           content::RenderFrameHost* parent_rfh) override;

  bool AreThirdPartyCookiesBlocked(const GURL& url,
                                   content::RenderFrameHost* rfh) override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void PersistClientHints(
      const url::Origin& primary_origin,
      content::RenderFrameHost* parent_rfh,
      const std::vector<::network::mojom::WebClientHintsType>& client_hints)
      override;

  void ResetForTesting() override;

  void SetAdditionalClientHints(
      const std::vector<network::mojom::WebClientHintsType>&) override;

  void ClearAdditionalClientHints() override;

  void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) override;
  gfx::Size GetMostRecentMainFrameViewportSize() override;

 private:
  const blink::UserAgentMetadata metadata_;
  ClientHintsContainer client_hints_map_;
  std::vector<network::mojom::WebClientHintsType> additional_hints_;
  gfx::Size viewport_size_;
  network::TestNetworkQualityTracker network_quality_tracker_;
};
}  // end namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
