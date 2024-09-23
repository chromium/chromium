// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"
#include "headless/lib/renderer/allowlist.h"

namespace headless {

class HeadlessContentRendererClient : public content::ContentRendererClient {
 public:
  HeadlessContentRendererClient();

  HeadlessContentRendererClient(const HeadlessContentRendererClient&) = delete;
  HeadlessContentRendererClient& operator=(
      const HeadlessContentRendererClient&) = delete;

  ~HeadlessContentRendererClient() override;

 private:
  // content::ContentRendererClient overrides.
  bool IsSupportedVideoType(const media::VideoType& type) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;

  std::optional<Allowlist> video_codecs_allowlist_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_CONTENT_RENDERER_CLIENT_H_
