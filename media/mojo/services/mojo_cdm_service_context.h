// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/unguessable_token.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class CdmContextRef;
class MojoCdmService;

// A class that creates, owns and manages all MojoCdmService instances.
class MEDIA_MOJO_EXPORT MojoCdmServiceContext {
 public:
  MojoCdmServiceContext();

  MojoCdmServiceContext(const MojoCdmServiceContext&) = delete;
  MojoCdmServiceContext& operator=(const MojoCdmServiceContext&) = delete;

  ~MojoCdmServiceContext();

  // Registers the |cdm_service| and returns a unique (per-process) CDM ID.
  base::UnguessableToken RegisterCdm(MojoCdmService* cdm_service);

  // Unregisters the CDM. Must be called before the CDM is destroyed.
  void UnregisterCdm(const base::UnguessableToken& cdm_id);

  // Returns the CdmContextRef associated with |cdm_id|.
  std::unique_ptr<CdmContextRef> GetCdmContextRef(
      const base::UnguessableToken& cdm_id);

 private:
  // A map between CDM ID and MojoCdmService.
  std::map<base::UnguessableToken, MojoCdmService*> cdm_services_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
