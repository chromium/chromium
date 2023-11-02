// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ORCHESTRATOR_TOOLBAR_ANIMATEE_H_
#define IOS_CHROME_BROWSER_UI_ORCHESTRATOR_TOOLBAR_ANIMATEE_H_

// Protocol defining an interface to trigger changes on the toolbar. Calling
// those methods should not start any animation.
@protocol ToolbarAnimatee<NSObject>

// Changes related to the Location Bar container.
- (void)expandLocationBar;
- (void)contractLocationBar;

// Changes related to the cancel button.
- (void)showCancelButton;
- (void)hideCancelButton;

// Changes related to the buttons displayed when the omnibox is not focused.
- (void)showControlButtons;
- (void)hideControlButtons;

@end

#endif  // IOS_CHROME_BROWSER_UI_ORCHESTRATOR_TOOLBAR_ANIMATEE_H_
