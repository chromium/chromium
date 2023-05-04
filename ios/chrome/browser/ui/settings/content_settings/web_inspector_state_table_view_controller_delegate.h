// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate for the  screen allowing the user to enable WebInspector support.
@protocol WebInspectorStateTableViewControllerDelegate

// Called when the user enables or disables WebInspector.
- (void)didEnableWebInspector:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_DELEGATE_H_
