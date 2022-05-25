// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "build/build_config.h"
#include "content/browser/metrics/histograms_internals_ui.h"
#include "content/browser/ukm_internals_ui.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return WebUI::kNoWebUI;

  if (url.host_piece() == kChromeUIHistogramHost ||
      url.host_piece() == kChromeUIUkmHost) {
    return const_cast<ContentWebUIControllerFactory*>(this);
  }
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
ContentWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                           const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;
  if (url.host_piece() == kChromeUIHistogramHost)
    return std::make_unique<HistogramsInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIUkmHost)
    return std::make_unique<UkmInternalsUI>(web_ui);
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
