// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_

#include "base/functional/callback.h"
#include "components/url_rewrite/renderer/url_request_rules_receiver.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrame;
}  // namespace content

// This class owns WebEngine-specific objects whose lifespan is tied to a
// RenderFrame. Owned by WebEngineContentRendererClient, this object will be
// destroyed on `blink::WebLocalFrame` destruction, triggering the destruction
// of all of the objects it exposes.
class WebEngineRenderFrameObserver final : public content::RenderFrameObserver {
 public:
  // |on_render_frame_deleted_callback| must delete |this|.
  WebEngineRenderFrameObserver(
      content::RenderFrame* render_frame,
      base::OnceCallback<void(const blink::LocalFrameToken&)>
          on_render_frame_deleted_callback);
  ~WebEngineRenderFrameObserver() override;

  WebEngineRenderFrameObserver(const WebEngineRenderFrameObserver&) = delete;
  WebEngineRenderFrameObserver& operator=(const WebEngineRenderFrameObserver&) =
      delete;

  url_rewrite::UrlRequestRulesReceiver* url_request_rules_receiver() {
    return &url_request_rules_receiver_;
  }

 private:
  // content::RenderFrameObserver implementation.
  void OnDestruct() override;
  void WillDetach(blink::DetachReason detach_reason) override;

  url_rewrite::UrlRequestRulesReceiver url_request_rules_receiver_;

  base::OnceCallback<void(const blink::LocalFrameToken&)>
      on_render_frame_deleted_callback_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_
