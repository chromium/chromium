// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
#define FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/synchronization/lock.h"
#include "content/public/browser/web_contents_binding_set.h"
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
class WEB_ENGINE_EXPORT UrlRequestRewriteRulesManager
    : public content::WebContentsObserver,
      public WebEngineURLLoaderThrottle::CachedRulesProvider {
 public:
  static UrlRequestRewriteRulesManager* ForFrameTreeNodeId(
      int frame_tree_node_id);

  static std::unique_ptr<UrlRequestRewriteRulesManager> CreateForTesting();

  explicit UrlRequestRewriteRulesManager(content::WebContents* web_contents);
  ~UrlRequestRewriteRulesManager() final;

  // Signals |rules| have been updated. Actual implementation for
  // fuchsia.web.Frame/SetUrlRequestRewriteRules.
  // Return ZX_OK on success and an error code otherwise.
  zx_status_t OnRulesUpdated(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
      fuchsia::web::Frame::SetUrlRequestRewriteRulesCallback callback);

  // WebEngineURLLoaderThrottle::CachedRulesProvider implementation.
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
  GetCachedRules() override;

 private:
  // Helper struct containing a RenderFrameHost and its corresponding
  // AssociatedRemote.
  struct ActiveFrame {
    ActiveFrame(content::RenderFrameHost* render_frame_host,
                mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver>
                    associated_remote);
    ActiveFrame(ActiveFrame&& other);
    ~ActiveFrame();

    content::RenderFrameHost* render_frame_host;
    mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver> associated_remote;
  };

  // Test-only constructor.
  explicit UrlRequestRewriteRulesManager();

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  base::Lock lock_;
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules_ GUARDED_BY(lock_);

  // Map of FrameTreeNode Ids to their current ActiveFrame.
  std::map<int, ActiveFrame> active_frames_;

  DISALLOW_COPY_AND_ASSIGN(UrlRequestRewriteRulesManager);
};

#endif  // FUCHSIA_ENGINE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
