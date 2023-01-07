// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_DEFAULT_CDM_FACTORY_H_
#define MEDIA_CDM_DEFAULT_CDM_FACTORY_H_

#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"

namespace media {

struct CdmConfig;

class MEDIA_EXPORT DefaultCdmFactory final : public CdmFactory {
 public:
  DefaultCdmFactory();

  DefaultCdmFactory(const DefaultCdmFactory&) = delete;
  DefaultCdmFactory& operator=(const DefaultCdmFactory&) = delete;

  ~DefaultCdmFactory() final;

  // CdmFactory implementation.
  void Create(const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              CdmCreatedCB cdm_created_cb) final;
};

}  // namespace media

#endif  // MEDIA_CDM_DEFAULT_CDM_FACTORY_H_
