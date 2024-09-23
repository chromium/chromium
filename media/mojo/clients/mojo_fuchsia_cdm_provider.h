// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_FUCHSIA_CDM_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_FUCHSIA_CDM_PROVIDER_H_

#include "media/cdm/fuchsia/fuchsia_cdm_provider.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class BrowserInterfaceBrokerProxy;
}

namespace media {

class MojoFuchsiaCdmProvider : public FuchsiaCdmProvider {
 public:
  // |interface_broker| must outlive this class.
  explicit MojoFuchsiaCdmProvider(
      const blink::BrowserInterfaceBrokerProxy* interface_broker);

  MojoFuchsiaCdmProvider(const MojoFuchsiaCdmProvider&) = delete;
  MojoFuchsiaCdmProvider& operator=(const MojoFuchsiaCdmProvider&) = delete;

  ~MojoFuchsiaCdmProvider() override;

  // FuchsiaCdmProvider implementation:
  void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          cdm_request) override;

 private:
  const blink::BrowserInterfaceBrokerProxy* const interface_broker_;
  mojo::Remote<media::mojom::FuchsiaMediaCdmProvider> cdm_provider_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_FUCHSIA_CDM_PROVIDER_H_
