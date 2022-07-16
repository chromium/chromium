// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_context.h"

#include "media/base/callback_registry.h"

namespace media {

CdmContext::CdmContext() = default;

CdmContext::~CdmContext() = default;

std::unique_ptr<CallbackRegistration> CdmContext::RegisterEventCB(
    EventCB /* event_cb */) {
  return nullptr;
}

Decryptor* CdmContext::GetDecryptor() {
  return nullptr;
}

absl::optional<base::UnguessableToken> CdmContext::GetCdmId() const {
  return absl::nullopt;
}

std::string CdmContext::CdmIdToString(const base::UnguessableToken* cdm_id) {
  return cdm_id ? cdm_id->ToString() : "null";
}

#if defined(OS_WIN)
bool CdmContext::RequiresMediaFoundationRenderer() {
  return false;
}

bool CdmContext::GetMediaFoundationCdmProxy(
    GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
  return false;
}
#endif

#if defined(OS_ANDROID)
MediaCryptoContext* CdmContext::GetMediaCryptoContext() {
  return nullptr;
}
#endif

#if defined(OS_FUCHSIA)
FuchsiaCdmContext* CdmContext::GetFuchsiaCdmContext() {
  return nullptr;
}
#endif

#if defined(OS_CHROMEOS)
chromeos::ChromeOsCdmContext* CdmContext::GetChromeOsCdmContext() {
  return nullptr;
}
#endif

}  // namespace media
