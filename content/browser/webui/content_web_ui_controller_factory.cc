// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "build/build_config.h"
#include "content/browser/appcache/appcache_internals_ui.h"
#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/histograms_internals_ui.h"
#include "content/browser/indexed_db/indexed_db_internals_ui.h"
#include "content/browser/media/media_internals_ui.h"
#include "content/browser/net/network_errors_listing_ui.h"
#include "content/browser/process_internals/process_internals_ui.h"
#include "content/browser/service_worker/service_worker_internals_ui.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/browser/webrtc/webrtc_internals_ui.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "media/media_buildflags.h"

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return WebUI::kNoWebUI;

  if (url.host_piece() == kChromeUIWebRTCInternalsHost ||
#if !defined(OS_ANDROID)
      url.host_piece() == kChromeUITracingHost ||
#endif
      url.host_piece() == kChromeUIGpuHost ||
      url.host_piece() == kChromeUIHistogramHost ||
      url.host_piece() == kChromeUIIndexedDBInternalsHost ||
      url.host_piece() == kChromeUIMediaInternalsHost ||
      url.host_piece() == kChromeUIServiceWorkerInternalsHost ||
      url.host_piece() == kChromeUIAppCacheInternalsHost ||
      url.host_piece() == kChromeUINetworkErrorsListingHost ||
      url.host_piece() == kChromeUIProcessInternalsHost) {
    return const_cast<ContentWebUIControllerFactory*>(this);
  }
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIBindingsForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return UseWebUIForURL(browser_context, url);
}

std::unique_ptr<WebUIController>
ContentWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                           const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  if (url.host_piece() == kChromeUIAppCacheInternalsHost)
    return std::make_unique<AppCacheInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIGpuHost)
    return std::make_unique<GpuInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIHistogramHost)
    return std::make_unique<HistogramsInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIIndexedDBInternalsHost)
    return std::make_unique<IndexedDBInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIMediaInternalsHost)
    return std::make_unique<MediaInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIServiceWorkerInternalsHost)
    return std::make_unique<ServiceWorkerInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUINetworkErrorsListingHost)
    return std::make_unique<NetworkErrorsListingUI>(web_ui);
#if !defined(OS_ANDROID)
  if (url.host_piece() == kChromeUITracingHost)
    return std::make_unique<TracingUI>(web_ui);
#endif
  if (url.host_piece() == kChromeUIWebRTCInternalsHost)
    return std::make_unique<WebRTCInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIProcessInternalsHost)
    return std::make_unique<ProcessInternalsUI>(web_ui);

  return nullptr;
}

// static
ContentWebUIControllerFactory* ContentWebUIControllerFactory::GetInstance() {
  return base::Singleton<ContentWebUIControllerFactory>::get();
}

ContentWebUIControllerFactory::ContentWebUIControllerFactory() {
}

ContentWebUIControllerFactory::~ContentWebUIControllerFactory() {
}

}  // namespace content
