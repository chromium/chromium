// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

@class SwitchContentView;

// Content configuration for a view with a single switch.
@interface SwitchContentConfiguration : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The view's switch is configured to call `selector` on `target`.
@property(nonatomic, weak) id target;
@property(nonatomic, assign) SEL selector;

// Whether the switch is on by default. Default it NO.
@property(nonatomic, assign) BOOL on;

// Whether the switch is enabled. Default is YES.
@property(nonatomic, assign) BOOL enabled;

// The tag for the switch.
@property(nonatomic, assign) NSInteger tag;

// LINT.ThenChange(switch_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_
