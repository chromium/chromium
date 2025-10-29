// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

// Content configuration for a view with a single info button.
@interface InfoButtonContentConfiguration
    : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The view's button is configured to call `selector` on `target`.
@property(nonatomic, weak) id target;
@property(nonatomic, assign) SEL selector;

// Whether the button is enabled. Default is YES.
@property(nonatomic, assign) BOOL enabled;

// The tag for the button.
@property(nonatomic, assign) NSInteger tag;

// Whether the button should be the target of VoiceOver taps. Default is YES.
@property(nonatomic, assign) BOOL selectedForVoiceOver;

// LINT.ThenChange(info_button_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_CONFIGURATION_H_
