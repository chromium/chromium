// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/client/mojo_fuchsia_cdm_provider.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace media {

MojoFuchsiaCdmProvider::MojoFuchsiaCdmProvider(
    service_manager::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
}

MojoFuchsiaCdmProvider::~MojoFuchsiaCdmProvider() = default;

void MojoFuchsiaCdmProvider::CreateCdmInterface(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        cdm_request) {
  if (!cdm_provider_) {
    interface_provider_->GetInterface(
        cdm_provider_.BindNewPipeAndPassReceiver());
  }

  cdm_provider_->CreateCdmInterface(key_system, std::move(cdm_request));
}

}  // namespace media
