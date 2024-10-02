// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/accessibility/model/window_accessibility_change_notifier_app_agent.h"

#import "base/check.h"
#import "base/i18n/message_formatter.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

namespace {

// Delay between events and notification.
const NSTimeInterval kWindowNotifcationDelay = 0.5;  // seconds

}  // namespace

@interface WindowAccessibilityChangeNotifierAppAgent () <AppStateObserver,
                                                         SceneStateObserver>
// Observed app state.
@property(nonatomic, weak) AppState* appState;

@property(nonatomic, assign) NSUInteger visibleWindowCount;

// If an update is pending, `lastUpdateTime` is the last time that an event
// occurred that might cause the window count to change. If no update is pending
// `lastUpdateTime` is nil.
@property(nonatomic, strong) NSDate* lastUpdateTime;

@end

@implementation WindowAccessibilityChangeNotifierAppAgent

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
  [self updateWindowCount];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

#pragma mark - SceneStateObserver

// Init stage changes are potential opportunities for dictating the window count
// to Voiceover users.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [self maybeScheduleWindowCountWithDelay:kWindowNotifcationDelay];
}

// Changes in the activation level of scene states will indicate that the count
// of visible windows has changed. Some actions (such as opening a third window
// when two are already open) can cause a scene's activation level to change
// at a time when other scenes' activation levels have not yet updated, which
// would cause notifications of incorrect window counts. To handle this type
// of change, this class instead posts notifications with a delay. If another
// notification is requested before a queued one executes, the new request is
// skipped.
- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.lastUpdateTime == nil) {
    [self maybeScheduleWindowCountWithDelay:kWindowNotifcationDelay];
  }
  self.lastUpdateTime = [NSDate date];
}

#pragma mark - private

- (void)maybeScheduleWindowCountWithDelay:(NSTimeInterval)delay {
  // Weakify, since the window count can change in shutdown, so there are
  // likely to be pending notifications that would otherwise keep this object
  // alive.
  if (self.appState.initStage == AppInitStage::kFinal) {
    __weak WindowAccessibilityChangeNotifierAppAgent* weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 static_cast<int64_t>(delay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
                     [weakSelf notifyWindowCount];
                   });
  }
}

// Performs the notification, if enough time has passed since the last update.
// If the last update was more recent than the notification delay, then the
// notification is re-posted to happen after the delay has elapsed.
- (void)notifyWindowCount {
  NSDate* now = [NSDate date];
  NSTimeInterval delta = [now timeIntervalSinceDate:self.lastUpdateTime];
  if (delta < kWindowNotifcationDelay) {
    // Repost with a delay sufficient to be `kWindowNotifcationDelay` after
    // the last update time.
    NSTimeInterval newDelta = kWindowNotifcationDelay - delta;
    [self maybeScheduleWindowCountWithDelay:newDelta];
    return;
  }

  if (!ui::ResourceBundle::HasSharedInstance()) {
    // The resources have not yet been initialized. Delay the notification.
    [self maybeScheduleWindowCountWithDelay:kWindowNotifcationDelay];
    return;
  }

  self.lastUpdateTime = nil;

  NSUInteger previousWindowCount = self.visibleWindowCount;
  [self updateWindowCount];

  // Only notify the user if (a) the window count has changed, and (b) it's
  // non-zero. A zero window count would occur, for example, when the user
  // enters the system app switcher. Other accessibility systems will notify
  // them of that change; it isn't necessary to tell them that no Chrome windows
  // are showing.
  if (previousWindowCount != self.visibleWindowCount &&
      self.visibleWindowCount > 0) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_WINDOW_COUNT_CHANGE);
    int numberOfWindows = static_cast<int>(self.visibleWindowCount);
    std::u16string formattedMessage =
        base::i18n::MessageFormatter::FormatWithNamedArgs(pattern, "count",
                                                          numberOfWindows);

    NSString* windowCountNotification =
        base::SysUTF16ToNSString(formattedMessage);
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    windowCountNotification);
  }
}

// Update `self.visibleWindowCount` with the total number of foregrounded
// connected scenes.
- (void)updateWindowCount {
  NSUInteger windowCount = 0;
  for (SceneState* scene in [self.appState connectedScenes]) {
    if (scene.activationLevel >= SceneActivationLevelForegroundInactive) {
      windowCount++;
    }
  }
  self.visibleWindowCount = windowCount;
}

@end
