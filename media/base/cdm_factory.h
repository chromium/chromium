// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_FACTORY_H_
#define MEDIA_BASE_CDM_FACTORY_H_

#include <string>

#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace url {
class Origin;
}

namespace media {

// Callback used when CDM is created. |error_message| only used if
// ContentDecryptionModule is null (i.e. CDM can't be created).
using CdmCreatedCB =
    base::OnceCallback<void(const scoped_refptr<ContentDecryptionModule>&,
                            const std::string& error_message)>;

struct CdmConfig;

class MEDIA_EXPORT CdmFactory {
 public:
  CdmFactory();

  CdmFactory(const CdmFactory&) = delete;
  CdmFactory& operator=(const CdmFactory&) = delete;

  virtual ~CdmFactory();

  // Creates a CDM for |cdm_config| and returns it through |cdm_created_cb|
  // asynchronously.
  virtual void Create(
      const CdmConfig& cdm_config,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb,
      CdmCreatedCB cdm_created_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_FACTORY_H_
