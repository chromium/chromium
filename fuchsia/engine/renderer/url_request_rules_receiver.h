// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
#define FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/renderer/render_frame_observer.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}  // namespace content

// Provides rewriting rules for network requests. UrlRequestRulesReceiver
// objects are owned by their respective WebEngineContentRendererClient and they
// will be destroyed on RenderFrame destruction. This is guaranteed to outlive
// any WebEngineURLLoaderThrottle that uses it as the RenderFrame destruction
// will have triggered the destruction of all pending
// WebEngineURLLoaderThrottles.
// This class should only be used on the IO thread, with the exception of the
// GetCachedRules() implementation, which can be called from any sequence.
class UrlRequestRulesReceiver
    : public mojom::UrlRequestRulesReceiver,
      public WebEngineURLLoaderThrottle::CachedRulesProvider,
      public content::RenderFrameObserver {
 public:
  UrlRequestRulesReceiver(
      content::RenderFrame* render_frame,
      base::OnceCallback<void(int)> on_render_frame_deleted_callback);
  ~UrlRequestRulesReceiver() override;

 private:
  void OnUrlRequestRulesReceiverAssociatedReceiver(
      mojo::PendingAssociatedReceiver<mojom::UrlRequestRulesReceiver> receiver);

  // mojom::UrlRequestRulesReceiver implementation.
  void OnRulesUpdated(
      std::vector<mojom::UrlRequestRewriteRulePtr> rules) override;

  // WebEngineURLLoaderThrottle::CachedRulesProvider implementation.
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
  GetCachedRules() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  base::Lock lock_;

  // This is accessed by WebEngineURLLoaderThrottles, which can be off-sequence
  // in the case of synchronous network requests.
  scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>
      cached_rules_ GUARDED_BY(lock_);

  mojo::AssociatedReceiver<mojom::UrlRequestRulesReceiver>
      url_request_rules_receiver_{this};

  base::OnceCallback<void(int)> on_render_frame_deleted_callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(UrlRequestRulesReceiver);
};

#endif  // FUCHSIA_ENGINE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
