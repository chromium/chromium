// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCESSIBILITY_MODEL_WINDOW_ACCESSIBILITY_CHANGE_NOTIFIER_APP_AGENT_H_
#define IOS_CHROME_BROWSER_ACCESSIBILITY_MODEL_WINDOW_ACCESSIBILITY_CHANGE_NOTIFIER_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"

// An app agent that montitors the number of visible (that is: active) scenes
// and provides accessibility notifications when this count changes.
@interface WindowAccessibilityChangeNotifierAppAgent : NSObject <AppStateAgent>
@end

#endif  // IOS_CHROME_BROWSER_ACCESSIBILITY_MODEL_WINDOW_ACCESSIBILITY_CHANGE_NOTIFIER_APP_AGENT_H_
