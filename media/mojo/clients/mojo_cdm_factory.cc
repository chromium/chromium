// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

namespace {

void OnCdmCreated(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb,
    mojo::PendingRemote<mojom::ContentDecryptionModule> cdm_remote,
    media::mojom::CdmContextPtr cdm_context,
    CreateCdmStatus status) {
  // Convert from a PendingRemote to Remote so we can verify that it is
  // connected, this will also check if |cdm_remote| is null.
  mojo::Remote<mojom::ContentDecryptionModule> remote(std::move(cdm_remote));
  if (!remote || !remote.is_connected() || !cdm_context) {
    std::move(cdm_created_cb).Run(nullptr, status);
    return;
  }

  std::move(cdm_created_cb)
      .Run(base::MakeRefCounted<MojoCdm>(
               std::move(remote), std::move(cdm_context), cdm_config,
               session_message_cb, session_closed_cb, session_keys_change_cb,
               session_expiration_update_cb),
           status);
}

}  // namespace

MojoCdmFactory::MojoCdmFactory(
    media::mojom::InterfaceFactory* interface_factory,
    KeySystems* key_systems)
    : interface_factory_(interface_factory), key_systems_(key_systems) {
  DCHECK(interface_factory_);
  DCHECK(key_systems_);
}

MojoCdmFactory::~MojoCdmFactory() = default;

void MojoCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DVLOG(2) << __func__ << ": cdm_config=" << cdm_config;

  // If AesDecryptor can be used, always use it here in the local process.
  // Note: We should not run AesDecryptor in the browser process except for
  // testing. See http://crbug.com/441957.
  // Note: Previously MojoRenderer doesn't work with local CDMs, this has
  // been solved by using DecryptingRenderer. See http://crbug.com/913775.
  if (key_systems_->CanUseAesDecryptor(cdm_config.key_system)) {
    scoped_refptr<ContentDecryptionModule> cdm(
        new AesDecryptor(session_message_cb, session_closed_cb,
                         session_keys_change_cb, session_expiration_update_cb));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), cdm,
                                  CreateCdmStatus::kSuccess));
    return;
  }

  // Use `mojo::WrapCallbackWithDefaultInvokeIfNotRun()` in case the CDM process
  // crashes.
  interface_factory_->CreateCdm(
      cdm_config,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnCdmCreated, cdm_config, session_message_cb,
                         session_closed_cb, session_keys_change_cb,
                         session_expiration_update_cb,
                         std::move(cdm_created_cb)),
          mojo::NullRemote(), nullptr, CreateCdmStatus::kDisconnectionError));
}

}  // namespace media
