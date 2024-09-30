// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_names.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

MediaDrmBridgeFactory::MediaDrmBridgeFactory(CreateFetcherCB create_fetcher_cb,
                                             CreateStorageCB create_storage_cb)
    : create_fetcher_cb_(std::move(create_fetcher_cb)),
      create_storage_cb_(std::move(create_storage_cb)) {
  DCHECK(create_fetcher_cb_);
  DCHECK(create_storage_cb_);
}

MediaDrmBridgeFactory::~MediaDrmBridgeFactory() {
  if (cdm_created_cb_)
    std::move(cdm_created_cb_)
        .Run(nullptr, CreateCdmStatus::kCdmCreationAborted);
}

void MediaDrmBridgeFactory::Create(
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    CdmCreatedCB cdm_created_cb) {
  DCHECK(MediaDrmBridge::IsKeySystemSupported(cdm_config.key_system));
  DCHECK(scheme_uuid_.empty()) << "This factory can only be used once.";

  scheme_uuid_ = MediaDrmBridge::GetUUID(cdm_config.key_system);
  DCHECK(!scheme_uuid_.empty());

  // Set security level.
  if (cdm_config.key_system == kWidevineKeySystem) {
    security_level_ = cdm_config.use_hw_secure_codecs
                          ? MediaDrmBridge::SECURITY_LEVEL_1
                          : MediaDrmBridge::SECURITY_LEVEL_3;
  } else if (media::IsExternalClearKey(cdm_config.key_system)) {
    security_level_ = MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
  } else if (!cdm_config.use_hw_secure_codecs) {
    // Assume other key systems require hardware-secure codecs and thus do not
    // support full compositing.
    auto error_message =
        cdm_config.key_system +
        " may require use_video_overlay_for_embedded_encrypted_video";
    NOTREACHED_IN_MIGRATION() << error_message;
    std::move(cdm_created_cb).Run(nullptr, CreateCdmStatus::kInvalidCdmConfig);
    return;
  }

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
  cdm_created_cb_ = std::move(cdm_created_cb);

  if (media::IsExternalClearKey(cdm_config.key_system)) {
    // We don't use storage in ClearKey so we return before creation, and
    // initialize the Media Drm Bridge with an empty string.
    CreateMediaDrmBridge("");
    return;
  }
  // MediaDrmStorage may be lazy created in MediaDrmStorageBridge.
  storage_ = std::make_unique<MediaDrmStorageBridge>();

  storage_->Initialize(
      create_storage_cb_,
      base::BindOnce(&MediaDrmBridgeFactory::OnStorageInitialized,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmBridgeFactory::OnStorageInitialized(bool success) {
  DCHECK(storage_);
  DVLOG(2) << __func__ << ": success = " << success
           << ", origin_id = " << storage_->origin_id();

  // MediaDrmStorageBridge should only be created on a successful Initialize().
  if (!success) {
    std::move(cdm_created_cb_)
        .Run(nullptr, CreateCdmStatus::kGetCdmOriginIdFailed);
    return;
  }

  CreateMediaDrmBridge(storage_->origin_id());
}

void MediaDrmBridgeFactory::CreateMediaDrmBridge(const std::string& origin_id) {
  DCHECK(!media_drm_bridge_);

  // Requires MediaCrypto so that it can be used by MediaCodec-based decoders.
  const bool requires_media_crypto = true;

  auto result = MediaDrmBridge::CreateInternal(
      scheme_uuid_, origin_id, security_level_, "User", requires_media_crypto,
      std::move(storage_), create_fetcher_cb_, session_message_cb_,
      session_closed_cb_, session_keys_change_cb_,
      session_expiration_update_cb_);

  if (!result.has_value()) {
    std::move(cdm_created_cb_).Run(nullptr, std::move(result).code());
    return;
  }

  media_drm_bridge_ = std::move(result).value();
  media_drm_bridge_->SetMediaCryptoReadyCB(base::BindOnce(
      &MediaDrmBridgeFactory::OnMediaCryptoReady, weak_factory_.GetWeakPtr()));
}

void MediaDrmBridgeFactory::OnMediaCryptoReady(
    JavaObjectPtr media_crypto,
    bool requires_secure_video_codec) {
  DCHECK(media_crypto);
  if (media_crypto->is_null()) {
    media_drm_bridge_ = nullptr;
    std::move(cdm_created_cb_)
        .Run(nullptr, CreateCdmStatus::kMediaCryptoNotAvailable);
    return;
  }

  std::move(cdm_created_cb_).Run(media_drm_bridge_, CreateCdmStatus::kSuccess);
}

}  // namespace media
