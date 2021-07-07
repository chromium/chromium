// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/content_suggestions_scheduler_app_state_agent.h"

#include "components/ntp_snippets/content_suggestions_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsSchedulerAppAgent ()

// Flag to keep track if we notified the service once at the cold app start.
@property(nonatomic, assign) BOOL hasNotifiedColdStart;

@end

@implementation ContentSuggestionsSchedulerAppAgent

#pragma mark - SceneStateObserver

- (void)appDidEnterForeground {
  [self notifyForeground];
}

#pragma mark - private

// Convenience getter for the content suggestion service.
- (ntp_snippets::ContentSuggestionsService*)service {
  if (!self.appState.mainBrowserState) {
    return nil;
  }
  return IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
      self.appState.mainBrowserState);
}

// Notify the serivce about the cold start.
- (void)notifyColdStart {
  if ([self service]) {
    if ([PreviousSessionInfo sharedInstance]
            .isFirstSessionAfterLanguageChange) {
      [self service]->ClearAllCachedSuggestions();
    }

    [self service]->remote_suggestions_scheduler() -> OnBrowserColdStart();
  }
}

// Notify the serivce when the app is brought to foreground. The first time this
// happens, also notify about a cold start.
- (void)notifyForeground {
  if (!self.hasNotifiedColdStart) {
    [self notifyColdStart];
    self.hasNotifiedColdStart = YES;
  }

  if ([self service]) {
    [self service]->remote_suggestions_scheduler() -> OnBrowserForegrounded();
  }
}

@end
