// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

class CdmProxy;
class CdmContextRef;
class MojoCdmService;
class MojoCdmProxyService;

// A class that creates, owns and manages all MojoCdmService instances.
class MEDIA_MOJO_EXPORT MojoCdmServiceContext {
 public:
  MojoCdmServiceContext();
  ~MojoCdmServiceContext();

  // Registers the |cdm_service| and returns a unique (per-process) CDM ID.
  int RegisterCdm(MojoCdmService* cdm_service);

  // Unregisters the CDM. Must be called before the CDM is destroyed.
  void UnregisterCdm(int cdm_id);

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // Registers the |cdm_proxy_service| and returns a unique (per-process) CDM
  // ID.
  int RegisterCdmProxy(MojoCdmProxyService* cdm_proxy_service);

  // Unregisters the CdmProxy. Must be called before the CdmProxy is destroyed.
  void UnregisterCdmProxy(int cdm_id);
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  // Returns the CdmContextRef associated with |cdm_id|.
  std::unique_ptr<CdmContextRef> GetCdmContextRef(int cdm_id);

 private:
  // A map between CDM ID and MojoCdmService.
  std::map<int, MojoCdmService*> cdm_services_;

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // A map between CDM ID and MojoCdmProxyService.
  std::map<int, MojoCdmProxyService*> cdm_proxy_services_;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  DISALLOW_COPY_AND_ASSIGN(MojoCdmServiceContext);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
