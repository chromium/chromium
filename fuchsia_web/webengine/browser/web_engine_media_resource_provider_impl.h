// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_IMPL_H_

#include <lib/fidl/cpp/interface_handle.h>

#include "content/public/browser/document_service.h"
#include "fuchsia_web/webengine/mojom/web_engine_media_resource_provider.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class WebEngineMediaResourceProviderImpl final
    : public content::DocumentService<mojom::WebEngineMediaResourceProvider> {
 public:
  ~WebEngineMediaResourceProviderImpl() override;

  WebEngineMediaResourceProviderImpl(
      const WebEngineMediaResourceProviderImpl&) = delete;
  WebEngineMediaResourceProviderImpl& operator=(
      const WebEngineMediaResourceProviderImpl&) = delete;
  WebEngineMediaResourceProviderImpl(
      const WebEngineMediaResourceProviderImpl&&) = delete;
  WebEngineMediaResourceProviderImpl& operator=(
      const WebEngineMediaResourceProviderImpl&&) = delete;

  static void Bind(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<mojom::WebEngineMediaResourceProvider> receiver);

 private:
  WebEngineMediaResourceProviderImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::WebEngineMediaResourceProvider> receiver);

  // mojom::WebEngineMediaResourceProvider:
  void ShouldUseAudioConsumer(ShouldUseAudioConsumerCallback callback) override;
  void CreateAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) override;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_IMPL_H_
