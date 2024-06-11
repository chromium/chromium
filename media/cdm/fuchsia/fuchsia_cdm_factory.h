// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_FACTORY_H_
#define MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/base/media_export.h"
#include "media/cdm/fuchsia/fuchsia_cdm_provider.h"

namespace media {

class MEDIA_EXPORT FuchsiaCdmFactory final : public CdmFactory {
 public:
  // |interface_provider| and |key_systems| must outlive this class.
  FuchsiaCdmFactory(std::unique_ptr<FuchsiaCdmProvider> provider,
                    KeySystems* key_systems);

  FuchsiaCdmFactory(const FuchsiaCdmFactory&) = delete;
  FuchsiaCdmFactory& operator=(const FuchsiaCdmFactory&) = delete;

  ~FuchsiaCdmFactory() override;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) override;

 private:
  void OnCdmReady(uint32_t creation_id,
                  CdmCreatedCB cdm_created_cb,
                  bool success,
                  CreateCdmStatus status);

  std::unique_ptr<FuchsiaCdmProvider> cdm_provider_;
  uint32_t creation_id_ = 0;

  // Map between creation id and pending cdms
  base::flat_map<uint32_t, scoped_refptr<ContentDecryptionModule>>
      pending_cdms_;

  // Non-owned
  raw_ptr<KeySystems> key_systems_;

  base::WeakPtrFactory<FuchsiaCdmFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_FACTORY_H_
