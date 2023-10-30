// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_util.h"

#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

// To get access to UseSessionSerializationOptimizations().
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

void ScheduleSaveSessionForBrowser(Browser* browser) {
  if (web::features::UseSessionSerializationOptimizations()) {
    // The SessionRestorationService already schedules a save as soon as
    // changes are detected to the state of any Browser, so there is no
    // action to take if asked to saved with a delay.
    return;
  }

  SessionRestorationBrowserAgent::FromBrowser(browser)->SaveSession(false);
}

void SaveSessionForBrowser(Browser* browser) {
  if (web::features::UseSessionSerializationOptimizations()) {
    ChromeBrowserState* browser_state = browser->GetBrowserState();
    SessionRestorationServiceFactory::GetForBrowserState(browser_state)
        ->SaveSessions();
  } else {
    SessionRestorationBrowserAgent::FromBrowser(browser)->SaveSession(true);
  }
}
