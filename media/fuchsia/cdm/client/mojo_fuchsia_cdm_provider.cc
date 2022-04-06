// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/client/mojo_fuchsia_cdm_provider.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace media {

MojoFuchsiaCdmProvider::MojoFuchsiaCdmProvider(
    blink::BrowserInterfaceBrokerProxy* interface_broker)
    : interface_broker_(interface_broker) {
  DCHECK(interface_broker_);
}

MojoFuchsiaCdmProvider::~MojoFuchsiaCdmProvider() = default;

void MojoFuchsiaCdmProvider::CreateCdmInterface(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        cdm_request) {
  if (!media_resource_provider_) {
    interface_broker_->GetInterface(
        media_resource_provider_.BindNewPipeAndPassReceiver());
  }

  media_resource_provider_->CreateCdm(key_system, std::move(cdm_request));
}

}  // namespace media
