// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_CDM_PROVIDER_SERVICE_H_
#define FUCHSIA_ENGINE_BROWSER_CDM_PROVIDER_SERVICE_H_

#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace media {
class FuchsiaCdmManager;
}  // namespace media

class CdmProviderService {
 public:
  CdmProviderService();
  ~CdmProviderService();

  CdmProviderService(const CdmProviderService&) = delete;
  CdmProviderService& operator=(const CdmProviderService&) = delete;

  void Bind(content::RenderFrameHost* frame_host,
            mojo::PendingReceiver<media::mojom::FuchsiaCdmProvider> receiver);

 private:
  std::unique_ptr<media::FuchsiaCdmManager> cdm_manager_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_CDM_PROVIDER_SERVICE_H_
