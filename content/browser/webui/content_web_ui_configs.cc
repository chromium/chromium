// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_configs.h"

#include <memory>

#include "content/browser/attribution_reporting/attribution_internals_ui.h"
#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/indexed_db/indexed_db_internals_ui.h"
#include "content/browser/media/media_internals_ui.h"
#include "content/browser/metrics/histograms_internals_ui.h"
#include "content/browser/network/network_errors_listing_ui.h"
#include "content/browser/private_aggregation/private_aggregation_internals_ui.h"
#include "content/browser/process_internals/process_internals_ui.h"
#include "content/browser/quota/quota_internals_ui.h"
#include "content/browser/service_worker/service_worker_internals_ui.h"
#include "content/browser/ukm_internals_ui.h"
#include "content/browser/webrtc/webrtc_internals_ui.h"
#include "content/public/browser/webui_config_map.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/browser/tracing/trace_report/trace_report_internals_ui.h"
#include "content/browser/tracing/tracing_ui.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "content/browser/xr/webxr_internals/webxr_internals_ui.h"
#endif

namespace content {

void RegisterContentWebUIConfigs() {
  auto& map = WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<AttributionInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<GpuInternalsUIConfig>());
  map.AddWebUIConfig(
      std::make_unique<indexed_db::IndexedDBInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<MediaInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<HistogramsInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<NetworkErrorsListingUIConfig>());
  map.AddWebUIConfig(std::make_unique<PrivateAggregationInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ProcessInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<QuotaInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<ServiceWorkerInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<UkmInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<WebRTCInternalsUIConfig>());

#if BUILDFLAG(ENABLE_VR)
  map.AddWebUIConfig(std::make_unique<WebXrInternalsUIConfig>());
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  map.AddWebUIConfig(std::make_unique<TraceReportInternalsUIConfig>());
  map.AddWebUIConfig(std::make_unique<TracingUIConfig>());
#endif
}

}  // namespace content
