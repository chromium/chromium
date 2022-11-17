// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_KEYBOARD_SHORTCUTS_MENU_APP_AGENT_H_
#define IOS_CHROME_APP_KEYBOARD_SHORTCUTS_MENU_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// The agent that manages the saving of the
// kEnableKeyboardShortcutsMenuForNextColdStart user defaults value.
@interface KeyboardShortcutsMenuAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_APP_KEYBOARD_SHORTCUTS_MENU_APP_AGENT_H_
