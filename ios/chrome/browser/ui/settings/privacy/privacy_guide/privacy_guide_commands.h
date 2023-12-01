// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COMMANDS_H_

// Protocol for commands triggered by pressing various button in the Privacy
// Guide UI.
@protocol PrivacyGuideCommands

// Displays the next step in the Privacy Guide flow.
- (void)showNextStep;

// Dismisses the Privacy Guide.
- (void)dismissGuide;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_COMMANDS_H_
