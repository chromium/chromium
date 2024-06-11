// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/default_cdm_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "media/cdm/aes_decryptor.h"
#include "url/origin.h"

namespace media {

DefaultCdmFactory::DefaultCdmFactory() = default;

DefaultCdmFactory::~DefaultCdmFactory() = default;

static bool ShouldCreateAesDecryptor(const std::string& key_system) {
  if (IsClearKey(key_system))
    return true;

  // Should create AesDecryptor to support External Clear Key key system.
  // This is used for testing.
  return base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting) &&
         IsExternalClearKey(key_system);
}

void DefaultCdmFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  if (!ShouldCreateAesDecryptor(cdm_config.key_system)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cdm_created_cb), nullptr,
                                  CreateCdmStatus::kUnsupportedKeySystem));
    return;
  }

  auto cdm = base::MakeRefCounted<AesDecryptor>(
      session_message_cb, session_closed_cb, session_keys_change_cb,
      session_expiration_update_cb);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cdm_created_cb), cdm,
                                CreateCdmStatus::kSuccess));
}

}  // namespace media
