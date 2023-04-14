// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/mock_library_cdm.h"

#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/cdm/library_cdm/cdm_host_proxy_impl.h"

namespace media {

namespace {
static MockLibraryCdm* g_mock_library_cdm = nullptr;
}  // namespace

// static
MockLibraryCdm* MockLibraryCdm::GetInstance() {
  return g_mock_library_cdm;
}

template <typename HostInterface>
MockLibraryCdm::MockLibraryCdm(HostInterface* host,
                               const std::string& key_system)
    : cdm_host_proxy_(std::make_unique<CdmHostProxyImpl<HostInterface>>(host)) {
}

MockLibraryCdm::~MockLibraryCdm() {
  DCHECK(g_mock_library_cdm);
  g_mock_library_cdm = nullptr;
}

CdmHostProxy* MockLibraryCdm::GetCdmHostProxy() {
  return cdm_host_proxy_.get();
}

void MockLibraryCdm::Initialize(bool allow_distinctive_identifier,
                                bool allow_persistent_state,
                                bool use_hw_secure_codecs) {
  cdm_host_proxy_->OnInitialized(true);
}

void* CreateMockLibraryCdm(int cdm_interface_version,
                           const char* key_system,
                           uint32_t key_system_size,
                           GetCdmHostFunc get_cdm_host_func,
                           void* user_data) {
  DVLOG(1) << __func__;
  DCHECK(!g_mock_library_cdm);

  std::string key_system_string(key_system, key_system_size);

  // We support CDM_10 and CDM_11.
  using CDM_10 = cdm::ContentDecryptionModule_10;
  using CDM_11 = cdm::ContentDecryptionModule_11;

  if (cdm_interface_version == CDM_10::kVersion) {
    CDM_10::Host* host = static_cast<CDM_10::Host*>(
        get_cdm_host_func(CDM_10::Host::kVersion, user_data));
    if (!host)
      return nullptr;

    DVLOG(1) << __func__ << ": Create ClearKeyCdm with CDM_10::Host.";
    g_mock_library_cdm = new MockLibraryCdm(host, key_system_string);
    return static_cast<CDM_10*>(g_mock_library_cdm);
  }

  if (cdm_interface_version == CDM_11::kVersion) {
    CDM_11::Host* host = static_cast<CDM_11::Host*>(
        get_cdm_host_func(CDM_11::Host::kVersion, user_data));
    if (!host)
      return nullptr;

    DVLOG(1) << __func__ << ": Create ClearKeyCdm with CDM_11::Host.";
    g_mock_library_cdm = new MockLibraryCdm(host, key_system_string);
    return static_cast<CDM_11*>(g_mock_library_cdm);
  }

  return nullptr;
}

}  // namespace media
