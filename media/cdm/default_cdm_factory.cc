// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/default_cdm_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/cdm/aes_decryptor.h"
#include "url/origin.h"

namespace media {

DefaultCdmFactory::DefaultCdmFactory() = default;

DefaultCdmFactory::~DefaultCdmFactory() = default;

static bool ShouldCreateAesDecryptor(const std::string& key_system) {
  if (CanUseAesDecryptor(key_system))
    return true;

  // Should create AesDecryptor to support External Clear Key key system.
  // This is used for testing.
  return base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting) &&
         IsExternalClearKey(key_system);
}

void DefaultCdmFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  if (security_origin.opaque()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  if (!ShouldCreateAesDecryptor(key_system)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(cdm_created_cb, nullptr, "Unsupported key system."));
    return;
  }

  scoped_refptr<ContentDecryptionModule> cdm(
      new AesDecryptor(session_message_cb, session_closed_cb,
                       session_keys_change_cb, session_expiration_update_cb));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(cdm_created_cb, cdm, ""));
}

}  // namespace media
