// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/media_mojo_export.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/remote_cdm_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Registers the |remote_context| and returns a unique (per-process) CDM ID.
  // This is used with out-of-process video decoding with HWDRM. We run
  // MojoCdmServiceContext in the GPU process which works with MojoCdmService.
  // We also run MojoCdmServiceContext in the Video Decoder process which
  // doesn't use MojoCdmService, which is why we need to deal with
  // RemoteCdmContext directly.
  base::UnguessableToken RegisterRemoteCdmContext(
      chromeos::RemoteCdmContext* remote_context);

  // Unregisters the RemoteCdmContext. Must be called before the
  // RemoteCdmContext is destroyed.
  void UnregisterRemoteCdmContext(const base::UnguessableToken& cdm_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns the CdmContextRef associated with |cdm_id|.
  std::unique_ptr<CdmContextRef> GetCdmContextRef(
      const base::UnguessableToken& cdm_id);

 private:
  // Lock for cdm_services_. Audio and video decoder may access it from
  // different threads.
  base::Lock cdm_services_lock_;
  // A map between CDM ID and MojoCdmService.
  std::map<base::UnguessableToken, raw_ptr<MojoCdmService, CtnExperimental>>
      cdm_services_ GUARDED_BY(cdm_services_lock_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A map between CDM ID and RemoteCdmContext.
  std::map<base::UnguessableToken,
           raw_ptr<chromeos::RemoteCdmContext, CtnExperimental>>
      remote_cdm_contexts_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_CONTEXT_H_
