// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/fuchsia/fuchsia_cdm_factory.h"

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/bind_post_task.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/fuchsia/fuchsia_cdm.h"
#include "media/cdm/fuchsia/fuchsia_cdm_provider.h"

namespace media {

FuchsiaCdmFactory::FuchsiaCdmFactory(
    std::unique_ptr<FuchsiaCdmProvider> cdm_provider,
    KeySystems* key_systems)
    : cdm_provider_(std::move(cdm_provider)), key_systems_(key_systems) {
  DCHECK(cdm_provider_);
  DCHECK(key_systems_);
}

FuchsiaCdmFactory::~FuchsiaCdmFactory() = default;

void FuchsiaCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  CdmCreatedCB bound_cdm_created_cb =
      base::BindPostTaskToCurrentDefault(std::move(cdm_created_cb));

  if (key_systems_->CanUseAesDecryptor(cdm_config.key_system)) {
    auto cdm = base::MakeRefCounted<AesDecryptor>(
        session_message_cb, session_closed_cb, session_keys_change_cb,
        session_expiration_update_cb);
    std::move(bound_cdm_created_cb)
        .Run(std::move(cdm), CreateCdmStatus::kSuccess);
    return;
  }

  fuchsia::media::drm::ContentDecryptionModulePtr cdm_ptr;
  auto cdm_request = cdm_ptr.NewRequest();

  FuchsiaCdm::SessionCallbacks callbacks;
  callbacks.message_cb = session_message_cb;
  callbacks.closed_cb = session_closed_cb;
  callbacks.keys_change_cb = session_keys_change_cb;
  callbacks.expiration_update_cb = session_expiration_update_cb;

  uint32_t creation_id = creation_id_++;

  auto cdm = base::MakeRefCounted<FuchsiaCdm>(
      std::move(cdm_ptr),
      base::BindOnce(&FuchsiaCdmFactory::OnCdmReady, weak_factory_.GetWeakPtr(),
                     creation_id, std::move(bound_cdm_created_cb)),
      std::move(callbacks));

  cdm_provider_->CreateCdmInterface(cdm_config.key_system,
                                    std::move(cdm_request));
  pending_cdms_.emplace(creation_id, std::move(cdm));
}

void FuchsiaCdmFactory::OnCdmReady(uint32_t creation_id,
                                   CdmCreatedCB cdm_created_cb,
                                   bool success,
                                   CreateCdmStatus status) {
  auto it = pending_cdms_.find(creation_id);
  CHECK(it != pending_cdms_.end(), base::NotFatalUntil::M130);
  scoped_refptr<ContentDecryptionModule> cdm = std::move(it->second);
  pending_cdms_.erase(it);
  std::move(cdm_created_cb).Run(success ? std::move(cdm) : nullptr, status);
}

}  // namespace media
