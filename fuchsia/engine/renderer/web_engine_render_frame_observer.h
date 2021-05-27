// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_

#include "base/callback.h"
#include "content/public/renderer/render_frame_observer.h"
#include "fuchsia/engine/renderer/url_request_rules_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

// This class owns WebEngine-specific objects whose lifespan is tied to a
// RenderFrame. Owned by WebEngineContentRendererClient, this object will be
// destroyed on RenderFrame destruction, triggering the destruction of all of
// the objects it exposes.
class WebEngineRenderFrameObserver : public content::RenderFrameObserver {
 public:
  // |on_render_frame_deleted_callback| must delete |this|.
  WebEngineRenderFrameObserver(
      content::RenderFrame* render_frame,
      base::OnceCallback<void(int)> on_render_frame_deleted_callback);
  ~WebEngineRenderFrameObserver() final;

  WebEngineRenderFrameObserver(const WebEngineRenderFrameObserver&) = delete;
  WebEngineRenderFrameObserver& operator=(const WebEngineRenderFrameObserver&) =
      delete;

  UrlRequestRulesReceiver* url_request_rules_receiver() {
    return &url_request_rules_receiver_;
  }

 private:
  // content::RenderFrameObserver implementation.
  void OnDestruct() final;

  UrlRequestRulesReceiver url_request_rules_receiver_;

  base::OnceCallback<void(int)> on_render_frame_deleted_callback_;
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_RENDER_FRAME_OBSERVER_H_
