// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"
#include "fuchsia/engine/renderer/url_request_rules_receiver.h"

class WebEngineContentRendererClient : public content::ContentRendererClient {
 public:
  WebEngineContentRendererClient();
  ~WebEngineContentRendererClient() override;

  // Returns the UrlRequestRulesReceiver corresponding to |render_frame_id|.
  UrlRequestRulesReceiver* GetUrlRequestRulesReceiverForRenderFrameId(
      int render_frame_id) const;

 private:
  // Called by UrlRequestRulesReceivers when their corresponding RenderFrame is
  // in the process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // content::ContentRendererClient overrides.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems)
      override;
  bool IsSupportedVideoType(const media::VideoType& type) override;
  std::unique_ptr<content::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType type) override;

  // Map of rules receivers per RenderFrame ID.
  std::map<int, std::unique_ptr<UrlRequestRulesReceiver>>
      url_request_receivers_by_id_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineContentRendererClient);
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
