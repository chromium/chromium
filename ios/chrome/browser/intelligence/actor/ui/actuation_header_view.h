// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationHeaderItem;

// Header view anchored above the actuation worklog.
//
// One secondary item:
// +----------------------------------------------------+
// | (*)  Task Title              [secondary] [primary] |
// |      Subtitle (optional)                           |
// +----------------------------------------------------+
//
// Two or more secondary items:
// +----------------------------------------------------+
// | (*)  Task Title              ( [a] [b] ) [primary] |
// |      Subtitle (optional)                           |
// +----------------------------------------------------+
// (*) = Icon, [x] = Accessory button, ( [a] [b] ) = Grouped capsule
@interface ActuationHeaderView : UIView

// Main title text.
@property(nonatomic, copy) NSString* title;

// Subtitle text displayed below the title.
@property(nonatomic, copy) NSString* subtitle;

// Whether actuation is currently in progress. Default is NO (static logo).
@property(nonatomic, assign, getter=isActuating) BOOL actuating;

// Primary accessory button. Setting nil removes the button from the header.
@property(nonatomic, strong) ActuationHeaderItem* primaryItem;

// Items displayed before `primaryItem`. A single item is shown as a standalone
// button; two or more are grouped in a capsule. Setting nil or an empty array
// removes them from the header.
@property(nonatomic, copy) NSArray<ActuationHeaderItem*>* secondaryItems;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Resets the header state, clearing all internal properties.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_HEADER_VIEW_H_
