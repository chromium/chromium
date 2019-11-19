// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service_context.h"

#include "base/logging.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/cdm_context_ref_impl.h"
#include "media/mojo/services/mojo_cdm_service.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/mojo/services/mojo_cdm_proxy_service.h"
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

namespace media {

namespace {

// Helper function to get the next unique (per-process) CDM ID to be assigned to
// a CDM or CdmProxy. It will be used to locate the CDM by the media players
// living in the same process.
int GetNextCdmId() {
  static int g_next_cdm_id = CdmContext::kInvalidCdmId + 1;
  return g_next_cdm_id++;
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
class CdmProxyContextRef : public CdmContextRef, public CdmContext {
 public:
  explicit CdmProxyContextRef(base::WeakPtr<CdmContext> cdm_context)
      : cdm_context_(cdm_context) {}
  ~CdmProxyContextRef() final {}

  // CdmContextRef implementation.
  CdmContext* GetCdmContext() final { return this; }

 private:
  // CdmContext implementation.
  std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb) final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return cdm_context_ ? cdm_context_->RegisterEventCB(std::move(event_cb))
                        : nullptr;
  }

  Decryptor* GetDecryptor() final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return cdm_context_ ? cdm_context_->GetDecryptor() : nullptr;
  }

  CdmProxyContext* GetCdmProxyContext() final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return cdm_context_ ? cdm_context_->GetCdmProxyContext() : nullptr;
  }

  base::WeakPtr<CdmContext> cdm_context_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CdmProxyContextRef);
};
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

}  // namespace

MojoCdmServiceContext::MojoCdmServiceContext() = default;

MojoCdmServiceContext::~MojoCdmServiceContext() = default;

int MojoCdmServiceContext::RegisterCdm(MojoCdmService* cdm_service) {
  DCHECK(cdm_service);
  int cdm_id = GetNextCdmId();
  cdm_services_[cdm_id] = cdm_service;
  DVLOG(1) << __func__ << ": CdmService registered with CDM ID " << cdm_id;
  return cdm_id;
}

void MojoCdmServiceContext::UnregisterCdm(int cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;
  DCHECK(cdm_services_.count(cdm_id));
  cdm_services_.erase(cdm_id);
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
int MojoCdmServiceContext::RegisterCdmProxy(
    MojoCdmProxyService* cdm_proxy_service) {
  DCHECK(cdm_proxy_service);
  int cdm_id = GetNextCdmId();
  cdm_proxy_services_[cdm_id] = cdm_proxy_service;
  DVLOG(1) << __func__ << ": CdmProxyService registered with CDM ID " << cdm_id;
  return cdm_id;
}

void MojoCdmServiceContext::UnregisterCdmProxy(int cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;
  DCHECK(cdm_proxy_services_.count(cdm_id));
  cdm_proxy_services_.erase(cdm_id);
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

std::unique_ptr<CdmContextRef> MojoCdmServiceContext::GetCdmContextRef(
    int cdm_id) {
  DVLOG(1) << __func__ << ": cdm_id = " << cdm_id;

  // Check all CDMs first.
  auto cdm_service = cdm_services_.find(cdm_id);
  if (cdm_service != cdm_services_.end()) {
    if (!cdm_service->second->GetCdm()->GetCdmContext()) {
      NOTREACHED() << "All CDMs should support CdmContext.";
      return nullptr;
    }
    return std::make_unique<CdmContextRefImpl>(cdm_service->second->GetCdm());
  }

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // Next check all CdmProxies.
  auto cdm_proxy_service = cdm_proxy_services_.find(cdm_id);
  if (cdm_proxy_service != cdm_proxy_services_.end()) {
    return std::make_unique<CdmProxyContextRef>(
        cdm_proxy_service->second->GetCdmContext());
  }
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  LOG(ERROR) << "CdmContextRef cannot be obtained for CDM ID: " << cdm_id;
  return nullptr;
}

}  // namespace media
