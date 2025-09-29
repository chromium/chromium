// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

@class SwitchContentConfiguration;

// A view that displays a single switch, leading-aligned.
@interface SwitchContentView : UIView <UIContentView>

// Returns the view, configured with `configuration`.
- (instancetype)initWithConfiguration:
    (SwitchContentConfiguration*)configuration;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_SWITCH_CONTENT_VIEW_H_
