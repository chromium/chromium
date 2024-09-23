// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service_context.h"

#include "base/logging.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/cdm_context_ref_impl.h"
#include "media/mojo/services/mojo_cdm_service.h"

namespace media {

namespace {

// Helper function to get the next unique (per-process) CDM ID to be assigned to
// a CDM. It will be used to locate the CDM by the media players living in the
// same process.
base::UnguessableToken GetNextCdmId() {
  return base::UnguessableToken::Create();
}

}  // namespace

MojoCdmServiceContext::MojoCdmServiceContext() = default;

MojoCdmServiceContext::~MojoCdmServiceContext() = default;

base::UnguessableToken MojoCdmServiceContext::RegisterCdm(
    MojoCdmService* cdm_service) {
  DCHECK(cdm_service);
  base::UnguessableToken cdm_id = GetNextCdmId();
  base::AutoLock guard(cdm_services_lock_);
  cdm_services_[cdm_id] = cdm_service;
  DVLOG(1) << __func__ << ": CdmService registered with CDM ID " << cdm_id;
  return cdm_id;
}

void MojoCdmServiceContext::UnregisterCdm(
    const base::UnguessableToken& cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;
  base::AutoLock guard(cdm_services_lock_);
  DCHECK(cdm_services_.count(cdm_id));
  cdm_services_.erase(cdm_id);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::UnguessableToken MojoCdmServiceContext::RegisterRemoteCdmContext(
    chromeos::RemoteCdmContext* remote_context) {
  DCHECK(remote_context);
  base::UnguessableToken cdm_id = GetNextCdmId();
  remote_cdm_contexts_[cdm_id] = remote_context;
  DVLOG(1) << __func__ << ": RemoteCdmContext registered with CDM ID "
           << cdm_id;
  return cdm_id;
}

void MojoCdmServiceContext::UnregisterRemoteCdmContext(
    const base::UnguessableToken& cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;
  DCHECK(remote_cdm_contexts_.count(cdm_id));
  remote_cdm_contexts_.erase(cdm_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<CdmContextRef> MojoCdmServiceContext::GetCdmContextRef(
    const base::UnguessableToken& cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;

  // Check all CDMs first.
  {
    base::AutoLock guard(cdm_services_lock_);
    auto cdm_service = cdm_services_.find(cdm_id);
    if (cdm_service != cdm_services_.end()) {
      if (!cdm_service->second->GetCdm()->GetCdmContext()) {
        NOTREACHED_IN_MIGRATION() << "All CDMs should support CdmContext.";
        return nullptr;
      }
      return std::make_unique<CdmContextRefImpl>(cdm_service->second->GetCdm());
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Try the remote contexts now.
  auto remote_context = remote_cdm_contexts_.find(cdm_id);
  if (remote_context != remote_cdm_contexts_.end())
    return remote_context->second->GetCdmContextRef();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  LOG(ERROR) << "CdmContextRef cannot be obtained for CDM ID: " << cdm_id;
  return nullptr;
}

}  // namespace media
