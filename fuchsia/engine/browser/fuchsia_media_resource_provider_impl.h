// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FUCHSIA_MEDIA_RESOURCE_PROVIDER_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_FUCHSIA_MEDIA_RESOURCE_PROVIDER_IMPL_H_

#include <lib/fidl/cpp/interface_handle.h>

#include "content/public/browser/document_service.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class FuchsiaMediaResourceProviderImpl final
    : public content::DocumentService<
          media::mojom::FuchsiaMediaResourceProvider> {
 public:
  ~FuchsiaMediaResourceProviderImpl() override;

  FuchsiaMediaResourceProviderImpl(const FuchsiaMediaResourceProviderImpl&) =
      delete;
  FuchsiaMediaResourceProviderImpl& operator=(
      const FuchsiaMediaResourceProviderImpl&) = delete;
  FuchsiaMediaResourceProviderImpl(const FuchsiaMediaResourceProviderImpl&&) =
      delete;
  FuchsiaMediaResourceProviderImpl& operator=(
      const FuchsiaMediaResourceProviderImpl&&) = delete;

  static void Bind(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);

 private:
  FuchsiaMediaResourceProviderImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);

  // media::mojom::FuchsiaMediaResourceProvider:
  void ShouldUseAudioConsumer(ShouldUseAudioConsumerCallback callback) override;
  void CreateAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) override;
  void CreateAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) override;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FUCHSIA_MEDIA_RESOURCE_PROVIDER_IMPL_H_
