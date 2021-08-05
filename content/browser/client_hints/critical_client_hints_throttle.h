// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_

#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CriticalCHRestart {
  kNavigationStarted = 0,
  kHeaderPresent = 1,
  kNavigationRestarted = 2,
  kMaxValue = kNavigationRestarted,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AcceptCHFrameRestart {
  kFramePresent = 0,
  kNavigationRestarted = 1,
  kMaxValue = kNavigationRestarted,
};

}  // namespace

namespace content {

class BrowserContext;
class ClientHintsControllerDelegate;

class CriticalClientHintsThrottle : public blink::URLLoaderThrottle {
 public:
  CriticalClientHintsThrottle(
      BrowserContext* context,

      ClientHintsControllerDelegate* client_hint_delegate,
      int frame_tree_node_id);
  ~CriticalClientHintsThrottle() override = default;

  // blink::URLLoaderThrottle
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer) override;
  void HandleAcceptCHFrameReceived(
      const GURL& url,
      const std::vector<network::mojom::WebClientHintsType>& accept_ch_frame)
      override;

 private:
  // Returns true if the extra `hints` are both not stored in the client hints
  // preferences and allowed to be sent to the given URL (from the given frame
  // tree node and so on). If returning true, the new name and values are added
  // to |modified_headers|.
  bool ShouldRestartWithHints(
      const GURL& url,
      const std::vector<network::mojom::WebClientHintsType>& hints,
      net::HttpRequestHeaders& modified_headers);

  BrowserContext* context_;
  ClientHintsControllerDelegate* client_hint_delegate_;
  int frame_tree_node_id_;

  // The ACCEPT_CH frame should only restart a navigation once. Once a redirect
  // is triggered, the `accept_ch_frame_redirect_` flag for the feature is set
  // to true. These ensure the navigation doesn't turn into an infinite loop
  // (this object should stay alive until the navigation is committed).
  bool accept_ch_frame_redirect_ = false;

  // Additional headers to add after redirect.
  net::HttpRequestHeaders additional_client_hints_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
