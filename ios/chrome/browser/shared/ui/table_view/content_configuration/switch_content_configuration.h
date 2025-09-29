// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@class SwitchContentView;

// Content configuration for a view with a single switch.
@interface SwitchContentConfiguration : NSObject <UIContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The view's switch is configured to call `selector` on `target`.
@property(nonatomic, weak) id target;
@property(nonatomic, assign) SEL selector;

// Whether the switch is on by default. Default it NO.
@property(nonatomic, assign) BOOL on;

// The tag for the switch.
@property(nonatomic, assign) NSInteger tag;

// LINT.ThenChange(switch_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_CONFIGURATION_H_
