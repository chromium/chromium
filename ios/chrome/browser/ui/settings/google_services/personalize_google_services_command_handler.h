// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COMMAND_HANDLER_H_

// Protocol to communicate user actions from the view controller to its
// coordinator.
@protocol PersonalizeGoogleServicesCommandHandler <NSObject>

// Opens the "Web & App Activity" dialog.
- (void)openWebAppActivityDialog;

// Open the "Linked Google Services" dialog.
- (void)openLinkedGoogleServicesDialog;

@end
#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COMMAND_HANDLER_H_
