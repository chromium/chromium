// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/fuchsia_cdm_factory.h"

#include "base/bind.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/fuchsia/cdm/fuchsia_cdm.h"
#include "media/fuchsia/cdm/fuchsia_cdm_provider.h"
#include "url/origin.h"

namespace media {

FuchsiaCdmFactory::FuchsiaCdmFactory(
    std::unique_ptr<FuchsiaCdmProvider> cdm_provider)
    : cdm_provider_(std::move(cdm_provider)) {
  DCHECK(cdm_provider_);
}

FuchsiaCdmFactory::~FuchsiaCdmFactory() = default;

void FuchsiaCdmFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  CdmCreatedCB bound_cdm_created_cb = BindToCurrentLoop(cdm_created_cb);

  if (security_origin.opaque()) {
    std::move(bound_cdm_created_cb).Run(nullptr, "Invalid origin.");
    return;
  }

  if (CanUseAesDecryptor(key_system)) {
    auto cdm = base::MakeRefCounted<AesDecryptor>(
        session_message_cb, session_closed_cb, session_keys_change_cb,
        session_expiration_update_cb);
    std::move(bound_cdm_created_cb).Run(std::move(cdm), "");
    return;
  }

  fuchsia::media::drm::ContentDecryptionModulePtr cdm_ptr;
  cdm_provider_->CreateCdmInterface(key_system, cdm_ptr.NewRequest());

  FuchsiaCdm::SessionCallbacks callbacks;
  callbacks.message_cb = session_message_cb;
  callbacks.closed_cb = session_closed_cb;
  callbacks.keys_change_cb = session_keys_change_cb;
  callbacks.expiration_update_cb = session_expiration_update_cb;

  auto cdm = base::MakeRefCounted<FuchsiaCdm>(std::move(cdm_ptr),
                                              std::move(callbacks));

  std::move(bound_cdm_created_cb).Run(std::move(cdm), "");
}

}  // namespace media
