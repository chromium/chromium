// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_
#define MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_

#include "base/macros.h"
#include "media/fuchsia/cdm/fuchsia_cdm_provider.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace service_manager {
class InterfaceProvider;
}

namespace media {

class MojoFuchsiaCdmProvider : public FuchsiaCdmProvider {
 public:
  // |interface_provider| must outlive this class.
  explicit MojoFuchsiaCdmProvider(
      service_manager::InterfaceProvider* interface_provider);
  ~MojoFuchsiaCdmProvider() override;

  // FuchsiaCdmProvider implementation:
  void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          cdm_request) override;

 private:
  service_manager::InterfaceProvider* const interface_provider_;
  mojo::Remote<media::mojom::FuchsiaCdmProvider> cdm_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoFuchsiaCdmProvider);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_CLIENT_MOJO_FUCHSIA_CDM_PROVIDER_H_
