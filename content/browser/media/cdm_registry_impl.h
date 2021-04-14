// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_registry.h"

namespace content {

struct CdmInfo;

class CONTENT_EXPORT CdmRegistryImpl : public CdmRegistry {
 public:
  // Returns the CdmRegistryImpl singleton.
  static CdmRegistryImpl* GetInstance();

  // CdmRegistry implementation.
  void Init() override;
  void RegisterCdm(const CdmInfo& info) override;
  const std::vector<CdmInfo>& GetAllRegisteredCdms() override;

 private:
  friend class CdmRegistryImplTest;
  friend class KeySystemSupportTest;

  CdmRegistryImpl();
  ~CdmRegistryImpl() override;

  // Resets `this` to a clean state for testing.
  void ResetForTesting();

  std::vector<CdmInfo> cdms_;

  DISALLOW_COPY_AND_ASSIGN(CdmRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_REGISTRY_IMPL_H_
