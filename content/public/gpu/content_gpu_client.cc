// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/gpu/content_gpu_client.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/cdm/cdm_proxy.h"
#endif

namespace content {

gpu::SyncPointManager* ContentGpuClient::GetSyncPointManager() {
  return nullptr;
}

gpu::SharedImageManager* ContentGpuClient::GetSharedImageManager() {
  return nullptr;
}

viz::VizCompositorThreadRunner*
ContentGpuClient::GetVizCompositorThreadRunner() {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
std::unique_ptr<media::CdmProxy> ContentGpuClient::CreateCdmProxy(
    const base::Token& cdm_guid) {
  return nullptr;
}
#endif

}  // namespace content
