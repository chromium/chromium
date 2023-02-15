// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CDM_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CDM_PROVIDER_IMPL_H_

#include "content/public/browser/document_service.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"

namespace content {

// Implements media::mojom::FuchsiaMediaCdmProvider by calling out to the
// fuchsia::media::drm::ContentDecryptionModule APIs.
//
// It helps renderer frame to create CDM.
class FuchsiaMediaCdmProviderImpl final
    : public DocumentService<media::mojom::FuchsiaMediaCdmProvider> {
 public:
  ~FuchsiaMediaCdmProviderImpl() override;

  FuchsiaMediaCdmProviderImpl(const FuchsiaMediaCdmProviderImpl&) = delete;
  FuchsiaMediaCdmProviderImpl& operator=(const FuchsiaMediaCdmProviderImpl&) =
      delete;

  // Used to make this service available in the BinderMap.
  static void Bind(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCdmProvider> receiver);

 private:
  FuchsiaMediaCdmProviderImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaCdmProvider> receiver);

  // media::mojom::FuchsiaMediaCdmProvider implementation.
  void CreateCdm(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_FUCHSIA_MEDIA_CDM_PROVIDER_IMPL_H_
