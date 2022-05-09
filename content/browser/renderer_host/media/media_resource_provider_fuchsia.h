// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_RESOURCE_PROVIDER_FUCHSIA_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_RESOURCE_PROVIDER_FUCHSIA_H_

#include "content/public/browser/document_service.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"

namespace content {

class MediaResourceProviderFuchsia final
    : public content::DocumentService<
          media::mojom::FuchsiaMediaResourceProvider> {
 public:
  ~MediaResourceProviderFuchsia() final;

  MediaResourceProviderFuchsia(const MediaResourceProviderFuchsia&) = delete;
  MediaResourceProviderFuchsia& operator=(const MediaResourceProviderFuchsia&) =
      delete;

  static void Bind(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);

 private:
  MediaResourceProviderFuchsia(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);

  // media::mojom::FuchsiaMediaResourceProvider implementation.
  void CreateCdm(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) final;
  void CreateVideoDecoder(
      media::VideoCodec codec,
      bool secure_memory,
      fidl::InterfaceRequest<fuchsia::media::StreamProcessor>
          stream_processor_request) final;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_RESOURCE_PROVIDER_FUCHSIA_H_
