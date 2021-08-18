// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/ios_chrome_hints_manager.h"

#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace optimization_guide {

IOSChromeHintsManager::IOSChromeHintsManager(
    web::BrowserState* browser_state,
    PrefService* pref_service,
    optimization_guide::OptimizationGuideStore* hint_store,
    optimization_guide::TopHostProvider* top_host_provider,
    optimization_guide::TabUrlProvider* tab_url_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker)
    : HintsManager(browser_state->IsOffTheRecord(),
                   GetApplicationContext()->GetApplicationLocale(),
                   pref_service,
                   hint_store,
                   top_host_provider,
                   tab_url_provider,
                   url_loader_factory,
                   network_connection_tracker,
                   /*push_notification_manager=*/nullptr) {}

}  // namespace optimization_guide
