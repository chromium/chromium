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
                          ? MediaDrmBridge::SECURITY_LEVEL_HW_SECURE_ALL
                          : MediaDrmBridge::SECURITY_LEVEL_SW_SECURE_CRYPTO;
  } else if (media::IsExternalClearKey(cdm_config.key_system)) {
    security_level_ = MediaDrmBridge::SECURITY_LEVEL_UNKNOWN;
  } else if (!cdm_config.use_hw_secure_codecs) {
    // Assume other key systems require hardware-secure codecs and thus do not
    // support full compositing.
    auto error_message =
        cdm_config.key_system +
        " may require use_video_overlay_for_embedded_encrypted_video";
    NOTREACHED() << error_message;
  }

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
  cdm_created_cb_ = std::move(cdm_created_cb);

  // Create MediaDrmBridge synchronously.
  // For ClearKey, we require media crypto immediately since we don't use
  // storage. For others, we set requires_media_crypto to false during
  // pre-allocation, as we don't have the origin ID yet.
  const bool is_clearkey = media::IsExternalClearKey(cdm_config.key_system);
  auto storage = std::make_unique<MediaDrmStorageBridge>();

  auto result = MediaDrmBridge::CreateInternal(
      scheme_uuid_, "", security_level_, "User",
      /*requires_media_crypto=*/is_clearkey, std::move(storage),
      create_fetcher_cb_, session_message_cb_, session_closed_cb_,
      session_keys_change_cb_, session_expiration_update_cb_);

  if (!result.has_value()) {
    std::move(cdm_created_cb_).Run(nullptr, std::move(result).code());
    return;
  }
  media_drm_bridge_ = std::move(result).value();

  if (is_clearkey) {
    media_drm_bridge_->SetMediaCryptoReadyCB(
        base::BindOnce(&MediaDrmBridgeFactory::OnMediaCryptoReady,
                       weak_factory_.GetWeakPtr()));
  } else {
    media_drm_bridge_->storage()->Initialize(
        create_storage_cb_,
        base::BindOnce(&MediaDrmBridgeFactory::OnStorageInitialized,
                       weak_factory_.GetWeakPtr()));
  }
}

void MediaDrmBridgeFactory::OnStorageInitialized(bool success) {
  DCHECK(media_drm_bridge_);
  DVLOG(2) << __func__ << ": success = " << success
           << ", origin_id = " << media_drm_bridge_->storage()->origin_id();

  // If storage initialization fails, discard the pre-allocated bridge and fail
  // creation.
  if (!success) {
    media_drm_bridge_ = nullptr;
    std::move(cdm_created_cb_)
        .Run(nullptr, CreateCdmStatus::kGetCdmOriginIdFailed);
    return;
  }

  media_drm_bridge_->CompleteInitialization(
      media_drm_bridge_->storage()->origin_id(),
      base::BindOnce(&MediaDrmBridgeFactory::OnMediaCryptoReady,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmBridgeFactory::OnMediaCryptoReady(
    base::android::ScopedJavaGlobalRef<jobject> media_crypto,
    bool requires_secure_video_codec) {
  DCHECK(media_crypto);

  if (!media_crypto) {
    media_drm_bridge_ = nullptr;
    std::move(cdm_created_cb_)
        .Run(nullptr, CreateCdmStatus::kMediaCryptoNotAvailable);
    return;
  }

  std::move(cdm_created_cb_).Run(media_drm_bridge_, CreateCdmStatus::kSuccess);
}

}  // namespace media
