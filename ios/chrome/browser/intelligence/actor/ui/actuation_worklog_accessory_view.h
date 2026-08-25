// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ACCESSORY_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationWorklogAccessoryItem;

// Interactive card view displaying an accessory item with an icon, title,
// optional subtitle, optional detail text, and an optional chevron.
@interface ActuationWorklogAccessoryView : UIControl

// Read-only access to the underlying accessory item model.
@property(nonatomic, strong, readonly)
    ActuationWorklogAccessoryItem* accessoryItem;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Configures the subview content and visibility using the given accessory item.
- (void)configureWithAccessoryItem:
    (ActuationWorklogAccessoryItem*)accessoryItem;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ACCESSORY_VIEW_H_
