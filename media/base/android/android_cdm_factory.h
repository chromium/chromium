// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ANDROID_CDM_FACTORY_H_
#define MEDIA_BASE_ANDROID_ANDROID_CDM_FACTORY_H_

#include <stdint.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/media_drm_bridge_factory.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "media/base/provision_fetcher.h"

namespace media {

struct CdmConfig;

class MEDIA_EXPORT AndroidCdmFactory final : public CdmFactory {
 public:
  AndroidCdmFactory(CreateFetcherCB create_fetcher_cb,
                    CreateStorageCB create_storage_cb);

  AndroidCdmFactory(const AndroidCdmFactory&) = delete;
  AndroidCdmFactory& operator=(const AndroidCdmFactory&) = delete;

  ~AndroidCdmFactory() override;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

 private:
  // Callback for MediaDrmBridgeFactory::Create().
  void OnCdmCreated(uint32_t creation_id,
                    const scoped_refptr<ContentDecryptionModule>& cdm,
                    CreateCdmStatus status);

  CreateFetcherCB create_fetcher_cb_;
  CreateStorageCB create_storage_cb_;

  uint32_t creation_id_ = 0;

  // Map between creation ID and PendingCreations.
  using PendingCreation =
      std::pair<std::unique_ptr<MediaDrmBridgeFactory>, CdmCreatedCB>;
  base::flat_map<uint32_t, PendingCreation> pending_creations_;

  base::WeakPtrFactory<AndroidCdmFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ANDROID_CDM_FACTORY_H_
