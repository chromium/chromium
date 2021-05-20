// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_capability.h"

namespace content {

class CONTENT_EXPORT CdmRegistryImpl : public CdmRegistry {
 public:
  // Returns the CdmRegistryImpl singleton.
  static CdmRegistryImpl* GetInstance();

  // CdmRegistry implementation.
  void Init() override;
  void RegisterCdm(const CdmInfo& info) override;

  // Returns CdmInfo registered for `key_system` and `robustness`. Returns null
  // if no CdmInfo is registered, or if the CdmInfo registered is invalid.
  std::unique_ptr<CdmInfo> GetCdmInfo(const std::string& key_system,
                                      CdmInfo::Robustness robustness);

  // Finalizes the CdmInfo corresponding to `key_system` and `robustness` if its
  // CdmCapability is null (lazy initialization). No-op if the CdmInfo does not
  // exist, or if the CdmInfo's CdmCapability is not null. The CdmInfo will be
  // removed if `cdm_capability` is null, since the CDM does not support any
  // capability. Returns whether the CdmInfo was successfully updated with a
  // valid CdmCapability.
  bool FinalizeCdmCapability(
      const std::string& key_system,
      CdmInfo::Robustness robustness,
      absl::optional<media::CdmCapability> cdm_capability);

  const std::vector<CdmInfo>& GetAllRegisteredCdmsForTesting();

 private:
  friend class CdmRegistryImplTest;
  friend class KeySystemSupportImplTest;

  CdmRegistryImpl();
  ~CdmRegistryImpl() override;

  // Resets `this` to a clean state for testing.
  void ResetForTesting();

  base::Lock lock_;
  std::vector<CdmInfo> cdms_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(CdmRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
