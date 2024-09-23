// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/key_systems.h"

namespace media {

namespace mojom {
class InterfaceFactory;
}

class MojoCdmFactory final : public CdmFactory {
 public:
  explicit MojoCdmFactory(media::mojom::InterfaceFactory* interface_factory,
                          KeySystems* key_systems);

  MojoCdmFactory(const MojoCdmFactory&) = delete;
  MojoCdmFactory& operator=(const MojoCdmFactory&) = delete;

  ~MojoCdmFactory() final;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) final;

 private:
  raw_ptr<media::mojom::InterfaceFactory> interface_factory_;
  // Non-owned
  raw_ptr<KeySystems> key_systems_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
