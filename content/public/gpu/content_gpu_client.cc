// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/gpu/content_gpu_client.h"

namespace content {

gpu::SyncPointManager* ContentGpuClient::GetSyncPointManager() {
  return nullptr;
}

gpu::SharedImageManager* ContentGpuClient::GetSharedImageManager() {
  return nullptr;
}

gpu::Scheduler* ContentGpuClient::GetScheduler() {
  return nullptr;
}

viz::VizCompositorThreadRunner*
ContentGpuClient::GetVizCompositorThreadRunner() {
  return nullptr;
}

}  // namespace content
