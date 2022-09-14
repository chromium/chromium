// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_FACTORY_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_drm_storage_bridge.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/base/provision_fetcher.h"

namespace media {

struct CdmConfig;

// A factory for creating MediaDrmBridge. Only one MediaDrmBridge can be created
// at any time.
class MEDIA_EXPORT MediaDrmBridgeFactory final : public CdmFactory {
 public:
  MediaDrmBridgeFactory(CreateFetcherCB create_fetcher_cb,
                        CreateStorageCB create_storage_cb);

  MediaDrmBridgeFactory(const MediaDrmBridgeFactory&) = delete;
  MediaDrmBridgeFactory& operator=(const MediaDrmBridgeFactory&) = delete;

  ~MediaDrmBridgeFactory() override;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

 private:
  // Callback for Initialize() on |storage_|.
  void OnStorageInitialized(bool success);

  // Creates |media_drm_bridge_|, and call SetMediaCryptoReadyCB() to wait for
  // MediaCrypto to be ready.
  void CreateMediaDrmBridge(const std::string& origin_id);

  // Callback for SetMediaCryptoReadyCB() on |media_drm_bridge_|.
  void OnMediaCryptoReady(JavaObjectPtr media_crypto,
                          bool requires_secure_video_codec);

  CreateFetcherCB create_fetcher_cb_;
  CreateStorageCB create_storage_cb_;

  std::vector<uint8_t> scheme_uuid_;

  MediaDrmBridge::SecurityLevel security_level_ =
      MediaDrmBridge::SECURITY_LEVEL_DEFAULT;

  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  // TODO(xhwang): Make CdmCreatedCB an OnceCallback.
  using CdmCreatedOnceCB = base::OnceCallback<CdmCreatedCB::RunType>;
  CdmCreatedOnceCB cdm_created_cb_;

  std::unique_ptr<MediaDrmStorageBridge> storage_;
  scoped_refptr<MediaDrmBridge> media_drm_bridge_;

  base::WeakPtrFactory<MediaDrmBridgeFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_FACTORY_H_
