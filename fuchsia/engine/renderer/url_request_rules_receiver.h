// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
#define FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_

#include "base/sequence_checker.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}  // namespace content

// Provides rewriting rules for network requests. Owned by
// WebEngineRenderFrameObserver, this object will be destroyed on RenderFrame
// destruction. This class should only be used on the IO thread.
class UrlRequestRulesReceiver : public mojom::UrlRequestRulesReceiver {
 public:
  explicit UrlRequestRulesReceiver(content::RenderFrame* render_frame);
  ~UrlRequestRulesReceiver() override;

  UrlRequestRulesReceiver(const UrlRequestRulesReceiver&) = delete;
  UrlRequestRulesReceiver& operator=(const UrlRequestRulesReceiver&) = delete;

  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>&
  GetCachedRules();

 private:
  void OnUrlRequestRulesReceiverAssociatedReceiver(
      mojo::PendingAssociatedReceiver<mojom::UrlRequestRulesReceiver> receiver);

  // mojom::UrlRequestRulesReceiver implementation.
  void OnRulesUpdated(std::vector<mojom::UrlRequestRulePtr> rules) override;

  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules_;
  mojo::AssociatedReceiver<mojom::UrlRequestRulesReceiver>
      url_request_rules_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
