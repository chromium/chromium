// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_configs.h"

#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/indexed_db/indexed_db_internals_ui.h"
#include "content/browser/service_worker/service_worker_internals_ui.h"
#include "content/browser/webrtc/webrtc_internals_ui.h"
#include "content/public/browser/webui_config_map.h"

namespace content {

void RegisterContentWebUIConfigs() {
  auto& map = WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<GpuInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<IndexedDBInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ServiceWorkerInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebRTCInternalsUIConfig>());
}

}  // namespace content
