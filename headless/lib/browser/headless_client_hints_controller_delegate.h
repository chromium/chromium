// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <vector>

#include "base/time/time.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_export.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "url/origin.h"

class GURL;

namespace blink {
class EnabledClientHints;
struct UserAgentMetadata;
}  // namespace blink

namespace network {
class NetworkQualityTracker;
}  // namespace network

namespace headless {

using ClientHintsContainer =
    std::map<const url::Origin, blink::EnabledClientHints>;

// Headless's CH implementation based off MockClientHintsControllerDelegate.
class HEADLESS_EXPORT HeadlessClientHintsControllerDelegate
    : public content::ClientHintsControllerDelegate {
 public:
  HeadlessClientHintsControllerDelegate();
  ~HeadlessClientHintsControllerDelegate() override;
  HeadlessClientHintsControllerDelegate(
      const HeadlessClientHintsControllerDelegate&) = delete;
  HeadlessClientHintsControllerDelegate& operator=(
      const HeadlessClientHintsControllerDelegate&) = delete;

  // content::ClientHintsControllerDelegate:
  ::network::NetworkQualityTracker* GetNetworkQualityTracker() override;

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

  void SetAdditionalClientHints(
      const std::vector<::network::mojom::WebClientHintsType>& client_hints)
      override;

  void ClearAdditionalClientHints() override;

  void SetMostRecentMainFrameViewportSize(
      const gfx::Size& viewport_size) override;
  gfx::Size GetMostRecentMainFrameViewportSize() override;

 private:
  ClientHintsContainer persist_hints_;
  std::vector<::network::mojom::WebClientHintsType> additional_hints_;
  // TODO(crbug.com/40257952): Allow customizing this.
  gfx::Size viewport_size_ = {800, 600};
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
