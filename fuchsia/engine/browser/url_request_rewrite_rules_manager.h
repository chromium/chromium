// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
#define FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/sequence_checker.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "fuchsia/engine/web_engine_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

// Adapts the UrlRequestRewrite FIDL API to be sent to the renderers over the
// over the UrlRequestRewrite Mojo API.
class WEB_ENGINE_EXPORT UrlRequestRewriteRulesManager final
    : public content::WebContentsObserver {
 public:
  static std::unique_ptr<UrlRequestRewriteRulesManager> CreateForTesting();

  explicit UrlRequestRewriteRulesManager(content::WebContents* web_contents);
  ~UrlRequestRewriteRulesManager() override;

  // Signals |rules| have been updated. Actual implementation for
  // fuchsia.web.Frame/SetUrlRequestRewriteRules.
  // Return ZX_OK on success and an error code otherwise.
  zx_status_t OnRulesUpdated(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
      fuchsia::web::Frame::SetUrlRequestRewriteRulesCallback callback);

  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>&
  GetCachedRules();

 private:
  // Test-only constructor.
  UrlRequestRewriteRulesManager();

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules_;

  // Map of GlobalRoutingID to their current associated remote.
  std::map<content::GlobalRenderFrameHostId,
           mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver>>
      active_remotes_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(UrlRequestRewriteRulesManager);
};

#endif  // FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
