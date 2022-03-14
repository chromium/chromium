// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
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
  ~CriticalClientHintsThrottle() override;

  // blink::URLLoaderThrottle
  void BeforeWillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer) override;

 private:
  // Returns true if the extra `hints` are both not stored in the client hints
  // preferences and allowed to be sent to the given URL (from the given frame
  // tree node and so on). If returning true, the new name and values are added
  // to |modified_headers|.
  bool ShouldRestartWithHints(
      const url::Origin& origin,
      const std::vector<network::mojom::WebClientHintsType>& hints,
      net::HttpRequestHeaders& modified_headers);

  raw_ptr<BrowserContext> context_;
  raw_ptr<ClientHintsControllerDelegate> client_hint_delegate_;
  int frame_tree_node_id_;

  // Ensure that there's only one restart per origin
  base::flat_set<url::Origin> restarted_origins_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
