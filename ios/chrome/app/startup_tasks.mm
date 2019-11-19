// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup_tasks.h"

#import <MediaPlayer/MediaPlayer.h>

#include "base/bind.h"
#include "build/branding_buildflags.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#include "ios/chrome/app/intents/SearchInChromeIntent.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/bookmarks/startup_task_runner_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#import "ios/chrome/browser/upgrade/upgrade_center.h"
#include "ios/chrome/grit/ios_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constants for deferred initilization of the profile start-up task runners.
NSString* const kStartProfileStartupTaskRunners =
    @"StartProfileStartupTaskRunners";
}  // namespace

@interface StartupTasks ()

// Performs browser state initialization tasks that don't need to happen
// synchronously at startup.
+ (void)performDeferredInitializationForBrowserState:
    (ios::ChromeBrowserState*)browserState;
// Called when UIApplicationWillResignActiveNotification is received.
- (void)applicationWillResignActiveNotification:(NSNotification*)notification;

@end

@implementation StartupTasks

#pragma mark - Public methods.

+ (void)scheduleDeferredBrowserStateInitialization:
    (ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  // Schedule the start of the profile deferred task runners.
  [[DeferredInitializationRunner sharedInstance]
      enqueueBlockNamed:kStartProfileStartupTaskRunners
                  block:^{
                    [self performDeferredInitializationForBrowserState:
                              browserState];
                  }];
}

- (void)initializeOmaha {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (tests_hook::DisableUpdateService())
    return;
  // Start omaha service. We only do this on official builds.
  OmahaService::Start(
      GetApplicationContext()->GetSharedURLLoaderFactory()->Clone(),
      base::BindRepeating(^(const UpgradeRecommendedDetails& details) {
        [[UpgradeCenter sharedInstance] upgradeNotificationDidOccur:details];
      }));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

- (void)registerForApplicationWillResignActiveNotification {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationWillResignActiveNotification:)
             name:UIApplicationWillResignActiveNotification
           object:nil];
}

- (void)donateIntents {
  SearchInChromeIntent* searchInChromeIntent =
      [[SearchInChromeIntent alloc] init];
  searchInChromeIntent.suggestedInvocationPhrase = l10n_util::GetNSString(
      IDS_IOS_INTENTS_SEARCH_IN_CHROME_INVOCATION_PHRASE);
  INInteraction* interaction =
      [[INInteraction alloc] initWithIntent:searchInChromeIntent response:nil];
  [interaction donateInteractionWithCompletion:^(NSError* _Nullable error){
  }];
}

#pragma mark - Private methods.

+ (void)performDeferredInitializationForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  ios::StartupTaskRunnerServiceFactory::GetForBrowserState(browserState)
      ->StartDeferredTaskRunners();
  ReadingListDownloadServiceFactory::GetForBrowserState(browserState)
      ->Initialize();
}

- (void)applicationWillResignActiveNotification:(NSNotification*)notification {
  // If the control center is displaying now-playing information from Chrome,
  // attempt to clear it so that the URL is no longer shown
  // (crbug.com/475820). The now-playing information will not be cleared if
  // it's from a different app.
  NSDictionary* nowPlayingInfo =
      [[MPNowPlayingInfoCenter defaultCenter] nowPlayingInfo];
  if (nowPlayingInfo[MPMediaItemPropertyTitle]) {
    // No need to clear playing info if media is being played but there is no
    // way to check if video or audio is playing in web view.
    [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:nil];
  }
}

@end
