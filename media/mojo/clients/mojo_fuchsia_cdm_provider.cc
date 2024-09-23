// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_fuchsia_cdm_provider.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace media {

MojoFuchsiaCdmProvider::MojoFuchsiaCdmProvider(
    const blink::BrowserInterfaceBrokerProxy* interface_broker)
    : interface_broker_(interface_broker) {
  DCHECK(interface_broker_);
}

MojoFuchsiaCdmProvider::~MojoFuchsiaCdmProvider() = default;

void MojoFuchsiaCdmProvider::CreateCdmInterface(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        cdm_request) {
  if (!cdm_provider_) {
    interface_broker_->GetInterface(cdm_provider_.BindNewPipeAndPassReceiver());
  }

  cdm_provider_->CreateCdm(key_system, std::move(cdm_request));
}

}  // namespace media
