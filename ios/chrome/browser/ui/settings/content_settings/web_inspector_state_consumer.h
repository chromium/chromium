// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer protocol for the screen allowing the user to choose whether to
// enable Web Inspector support.
@protocol WebInspectorStateConsumer

// Sets whether Web Inspector is enabled.
- (void)setWebInspectorEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_CONSUMER_H_
