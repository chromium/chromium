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

  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  BrowserContext* context_;
  ClientHintsControllerDelegate* client_hint_delegate_;
  int frame_tree_node_id_;
  // This ensures the navigation doesn't turn into an infinite loop (this
  // object should stay alive until the navigation is committed). On finding a
  // critical client hint is missing, the throttle instigates an internal
  // redirect.
  bool redirected_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CRITICAL_CLIENT_HINTS_THROTTLE_H_
