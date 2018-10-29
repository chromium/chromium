// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_proxy_context.h"

#include "build/build_config.h"

namespace media {

CdmProxyContext::CdmProxyContext() {}
CdmProxyContext::~CdmProxyContext() {}

#if defined(OS_WIN)
base::Optional<CdmProxyContext::D3D11DecryptContext>
CdmProxyContext::GetD3D11DecryptContext(CdmProxy::KeyType key_type,
                                        const std::string& key_id) {
  return base::nullopt;
}
#endif

}  // namespace media