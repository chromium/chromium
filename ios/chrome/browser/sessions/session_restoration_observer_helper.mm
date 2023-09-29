// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_observer_helper.h"

#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/web/common/features.h"

void AddSessionRestorationObserver(Browser* browser,
                                   SessionRestorationObserver* observer) {
  if (web::features::UseSessionSerializationOptimizations()) {
    SessionRestorationServiceFactory::GetForBrowserState(
        browser->GetBrowserState())
        ->AddObserver(observer);
  } else {
    // The SessionRestorationBrowserAgent may not be created during unit tests,
    // so only perform the registration if it has been created.
    if (auto* agent = SessionRestorationBrowserAgent::FromBrowser(browser)) {
      agent->AddObserver(observer);
    }
  }
}

void RemoveSessionRestorationObserver(Browser* browser,
                                      SessionRestorationObserver* observer) {
  if (web::features::UseSessionSerializationOptimizations()) {
    SessionRestorationServiceFactory::GetForBrowserState(
        browser->GetBrowserState())
        ->RemoveObserver(observer);
  } else {
    // The SessionRestorationBrowserAgent may not be created during unit tests,
    // so only perform the unregistration if it has been created.
    if (auto* agent = SessionRestorationBrowserAgent::FromBrowser(browser)) {
      agent->RemoveObserver(observer);
    }
  }
}
