// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup_tasks.h"

#import <MediaPlayer/MediaPlayer.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/omaha/model/omaha_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/common/intents/SearchInChromeIntent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

@interface StartupTasks ()

// Called when UIApplicationWillResignActiveNotification is received.
- (void)applicationWillResignActiveNotification:(NSNotification*)notification;

@end

@implementation StartupTasks

#pragma mark - Public methods.

- (void)initializeOmaha {
  OmahaService::Start(
      GetApplicationContext()->GetSharedURLLoaderFactory()->Clone(),
      base::DoNothing());
}

- (void)registerForApplicationWillResignActiveNotification {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationWillResignActiveNotification:)
             name:UIApplicationWillResignActiveNotification
           object:nil];
}

- (void)logSiriShortcuts {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        [[INVoiceShortcutCenter sharedCenter]
            getAllVoiceShortcutsWithCompletion:^(
                NSArray<INVoiceShortcut*>* voiceShortcuts, NSError* error) {
              if (error || !voiceShortcuts) {
                return;
              }

              // The 20 shortcuts cap is arbitrary but seems like a reasonable
              // limit.
              base::UmaHistogramExactLinear(
                  "IOS.SiriShortcuts.Count",
                  base::saturated_cast<int>([voiceShortcuts count]), 20);
            }];
      }));
}

#pragma mark - Private methods.

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
