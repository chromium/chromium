// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/accessibility/model/window_accessibility_change_notifier_app_agent.h"

#import <optional>

#import "base/check.h"
#import "base/i18n/message_formatter.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

namespace {

// Delay between events and notification.
const base::TimeDelta kWindowNotifcationDelay = base::Seconds(0.5);

}  // namespace

@interface WindowAccessibilityChangeNotifierAppAgent () <ProfileStateObserver>
@end

@implementation WindowAccessibilityChangeNotifierAppAgent {
  // The timer used to delay the notifications.
  base::OneShotTimer _timer;

  // The number of visible scenes.
  int _visibleWindowCount;
}

#pragma mark - ProfileStateObserver

// Profile init stage changes are potential opportunities for dictating the
// window count to Voiceover users.
- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    [self maybeScheduleWindowCountWithDelay];
    [profileState removeObserver:self];
  }
}

#pragma mark - SceneStateObserver

// App init stage changes are potential opportunities for dictating the window
// count to Voiceover users.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [super appState:appState didTransitionFromInitStage:previousInitStage];
  [self maybeScheduleWindowCountWithDelay];
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
  [super sceneState:sceneState transitionedToActivationLevel:level];
  [self maybeScheduleWindowCountWithDelay];
}

// If the SceneState reaches SceneActivationLevelForegroundActive before the
// ProfileState is set, it would not have been possible to observe it in the
// -visibleWindowCount method (as it would be nil). Listening to this method
// allow to deal with this rare occurrence.
- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  [self maybeScheduleWindowCountWithDelay];
}

#pragma mark - Private methods

- (void)maybeScheduleWindowCountWithDelay {
  if (self.appState.initStage != AppInitStage::kFinal) {
    return;
  }

  // The resources have not yet been initialized. Ignore the notification.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    return;
  }

  // If there is already an update in-flight, ignore the current one.
  if (_timer.IsRunning()) {
    return;
  }

  // Weakify, since the window count can change in shutdown, so there are
  // likely to be pending notifications that would otherwise keep this object
  // alive.
  __weak WindowAccessibilityChangeNotifierAppAgent* weakSelf = self;
  _timer.Start(FROM_HERE, kWindowNotifcationDelay, base::BindOnce(^{
                 [weakSelf notifyWindowCount];
               }));
}

// Performs the notification, if enough time has passed since the last update.
// If the last update was more recent than the notification delay, then the
// notification is re-posted to happen after the delay has elapsed.
- (void)notifyWindowCount {
  // Only notify the user if (a) the window count has changed, and (b) it's
  // non-zero. A zero window count would occur, for example, when the user
  // enters the system app switcher. Other accessibility systems will notify
  // them of that change; it isn't necessary to tell them that no Chrome windows
  // are showing.
  const int visibleWindowCount = [self visibleWindowCount];
  if (visibleWindowCount <= 0 || visibleWindowCount == _visibleWindowCount) {
    return;
  }

  _visibleWindowCount = visibleWindowCount;
  std::u16string formattedMessage =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(IDS_IOS_WINDOW_COUNT_CHANGE), "count",
          _visibleWindowCount);

  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  base::SysUTF16ToNSString(formattedMessage));
}

// Counts the number of foregrounded fully connected scenes (i.e. whose
// profile is fully initialized).
- (int)visibleWindowCount {
  int windowCount = 0;
  for (SceneState* scene in self.appState.connectedScenes) {
    // Window is in background.
    if (scene.activationLevel < SceneActivationLevelForegroundInactive) {
      continue;
    }

    // Window's profile has not been fully initialized yet, skip it, but
    // observe the ProfileState to detect when its initialisation is over.
    if (scene.profileState.initStage < ProfileInitStage::kFinal) {
      [scene.profileState addObserver:self];
      continue;
    }

    ++windowCount;
  }

  return windowCount;
}

@end
