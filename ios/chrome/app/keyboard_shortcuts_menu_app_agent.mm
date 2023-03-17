// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/keyboard_shortcuts_menu_app_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/keyboard/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation KeyboardShortcutsMenuAppAgent

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (appState.initStage == InitStageBrowserObjectsForBackgroundHandlers) {
    // Save the value of the feature flag now since 'base::FeatureList' was
    // not available in ealier stages.
    // IsKeyboardShortcutsMenuEnabled() simply reads the saved value saved by
    // SaveKeyboardShortcutsMenuEnabledForNextColdStart(). Do not wrap this in
    // IsKeyboardShortcutsMenuEnabled() -- in this case, a new value would
    // never be saved again once we save NO, since the NO codepath would not
    // execute saving a new value.
    SaveKeyboardShortcutsMenuEnabledForNextColdStart();
  }
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

@end
