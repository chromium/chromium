// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Name of histogram to record the number of excess NTP tabs that are removed.
const char kExcessNTPTabsRemoved[] = "IOS.NTP.ExcessRemovedTabCount";
}  // namespace

@interface StartSurfaceSceneAgent ()

// Caches the previous activation level.
@property(nonatomic, assign) SceneActivationLevel previousActivationLevel;

@end

@implementation StartSurfaceSceneAgent

- (id)init {
  self = [super init];
  if (self) {
    self.previousActivationLevel = SceneActivationLevelUnattached;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level != SceneActivationLevelForegroundActive &&
      self.previousActivationLevel == SceneActivationLevelForegroundActive) {
    // TODO(crbug.com/1173160): Consider when to clear the session object since
    // Chrome may be closed without transiting to inactive, e.g. device power
    // off, then the previous session object is staled.
    SetStartSurfaceSessionObjectForSceneState(sceneState);
  }
  if (level <= SceneActivationLevelBackground &&
      self.previousActivationLevel > SceneActivationLevelBackground) {
    if (base::FeatureList::IsEnabled(kRemoveExcessNTPs)) {
      // Remove duplicate NTP pages upon background event.
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .mainInterface.browser];
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .incognitoInterface.browser];
    }
  }
  self.previousActivationLevel = level;
}

// Removes duplicate NTP tabs in |browser|'s WebStateList.
- (void)removeExcessNTPsInBrowser:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  NSMutableArray<NSNumber*>* emptyNtpIndices = [[NSMutableArray alloc] init];
  BOOL keepOneNTP = YES;
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    if (IsURLNtp(webState->GetVisibleURL())) {
      if (webState->GetNavigationManager()->GetItemCount() <= 1) {
        // Insert at the front so that iterating through the array will remove
        // WebStates in descending index order, preventing WebState indices from
        // changing during removal.
        [emptyNtpIndices insertObject:@(i) atIndex:0];
      } else {
        keepOneNTP = NO;
      }
    }
  }
  if (keepOneNTP) {
    // Remove last index as the one NTP to keep.
    [emptyNtpIndices removeLastObject];
  }
  UMA_HISTOGRAM_COUNTS_100(kExcessNTPTabsRemoved, [emptyNtpIndices count]);
  // Removal starts from higher indices to ensure tab indices stay fixed
  // throughout removal process.
  for (NSNumber* index in emptyNtpIndices) {
    web::WebState* webState =
        browser->GetWebStateList()->GetWebStateAt([index intValue]);
    DCHECK(IsURLNtp(webState->GetVisibleURL()));
    browser->GetWebStateList()->CloseWebStateAt([index intValue],
                                                WebStateList::CLOSE_NO_FLAGS);
  }
}

@end
