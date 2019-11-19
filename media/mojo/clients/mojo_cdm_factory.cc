// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "url/origin.h"

namespace media {

MojoCdmFactory::MojoCdmFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoCdmFactory::~MojoCdmFactory() = default;

void MojoCdmFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DVLOG(2) << __func__ << ": " << key_system;

  if (security_origin.opaque()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  // If AesDecryptor can be used, always use it here in the local process.
  // Note: We should not run AesDecryptor in the browser process except for
  // testing. See http://crbug.com/441957.
  // Note: Previously MojoRenderer doesn't work with local CDMs, this has
  // been solved by using DecryptingRenderer. See http://crbug.com/913775.
  if (CanUseAesDecryptor(key_system)) {
    scoped_refptr<ContentDecryptionModule> cdm(
        new AesDecryptor(session_message_cb, session_closed_cb,
                         session_keys_change_cb, session_expiration_update_cb));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(cdm_created_cb, cdm, ""));
    return;
  }

  mojo::PendingRemote<mojom::ContentDecryptionModule> cdm_pending_remote;
  interface_factory_->CreateCdm(
      key_system, cdm_pending_remote.InitWithNewPipeAndPassReceiver());

  MojoCdm::Create(key_system, security_origin, cdm_config,
                  std::move(cdm_pending_remote), interface_factory_,
                  session_message_cb, session_closed_cb, session_keys_change_cb,
                  session_expiration_update_cb, cdm_created_cb);
}

}  // namespace media
