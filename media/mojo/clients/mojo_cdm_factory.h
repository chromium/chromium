// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_

#include "base/macros.h"
#include "media/base/cdm_factory.h"

namespace media {

namespace mojom {
class InterfaceFactory;
}

class MojoCdmFactory final : public CdmFactory {
 public:
  explicit MojoCdmFactory(media::mojom::InterfaceFactory* interface_factory);
  ~MojoCdmFactory() final;

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) final;

 private:
  media::mojom::InterfaceFactory* interface_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
