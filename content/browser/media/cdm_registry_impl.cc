// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_registry_impl.h"

#include <stddef.h>

#include "base/logging.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#include "media/base/key_system_names.h"

namespace content {

namespace {

bool MatchKeySystem(const CdmInfo& cdm_info, const std::string& key_system) {
  return cdm_info.key_system == key_system ||
         (cdm_info.supports_sub_key_systems &&
          media::IsSubKeySystemOf(key_system, cdm_info.key_system));
}

}  // namespace

// static
CdmRegistry* CdmRegistry::GetInstance() {
  return CdmRegistryImpl::GetInstance();
}

// static
CdmRegistryImpl* CdmRegistryImpl::GetInstance() {
  static CdmRegistryImpl* registry = new CdmRegistryImpl();
  return registry;
}

CdmRegistryImpl::CdmRegistryImpl() {}

CdmRegistryImpl::~CdmRegistryImpl() {}

void CdmRegistryImpl::Init() {
  base::AutoLock auto_lock(lock_);

  // Let embedders register CDMs.
  GetContentClient()->AddContentDecryptionModules(&cdms_, nullptr);
}

void CdmRegistryImpl::RegisterCdm(const CdmInfo& info) {
  base::AutoLock auto_lock(lock_);

  // Always register new CDMs at the end of the list, so that the behavior is
  // consistent across the browser process's lifetime. For example, we'll always
  // use the same registered CDM for a given key system. This also means that
  // some later registered CDMs (component updated) will not be used until
  // browser restart, which is fine in most cases.
  cdms_.push_back(info);
}

std::unique_ptr<CdmInfo> CdmRegistryImpl::GetCdmInfo(
    const std::string& key_system,
    CdmInfo::Robustness robustness) {
  base::AutoLock auto_lock(lock_);
  for (const auto& cdm : cdms_) {
    if (cdm.robustness == robustness && MatchKeySystem(cdm, key_system))
      return std::make_unique<CdmInfo>(cdm);
  }

  return nullptr;
}

bool CdmRegistryImpl::FinalizeCdmCapability(
    const std::string& key_system,
    CdmInfo::Robustness robustness,
    absl::optional<media::CdmCapability> cdm_capability) {
  base::AutoLock auto_lock(lock_);

  auto itr = cdms_.begin();
  for (; itr != cdms_.end(); itr++) {
    if (itr->robustness == robustness && MatchKeySystem(*itr, key_system))
      break;
  }

  if (itr == cdms_.end()) {
    DVLOG(1) << __func__ << ": Cannot find CdmInfo to finalize";
    return false;
  }

  if (itr->capability) {
    DVLOG(1) << __func__ << ": CdmCapability already finalized";
    return false;
  }

  if (!cdm_capability) {
    DVLOG(1) << __func__ << ": No CdmCapability supported. Removing CdmInfo!";
    cdms_.erase(itr);
    return false;
  }

  itr->capability = cdm_capability.value();
  return true;
}

const std::vector<CdmInfo>& CdmRegistryImpl::GetRegisteredCdms() {
  base::AutoLock auto_lock(lock_);
  return cdms_;
}

void CdmRegistryImpl::ResetForTesting() {
  base::AutoLock auto_lock(lock_);
  cdms_.clear();
}

}  // namespace content
