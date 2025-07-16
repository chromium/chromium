// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FAKE_FACE_PILE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FAKE_FACE_PILE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_consumer.h"

// Fake object that implements FacePileConsumer.
@interface FakeFacePileConsumer : NSObject <FacePileConsumer>

@property(nonatomic, assign) BOOL lastShowsShareButtonWhenEmpty;
@property(nonatomic, strong) UIColor* lastFacePileBackgroundColor;
@property(nonatomic, assign) CGFloat lastAvatarSize;
@property(nonatomic, strong) NSArray<id<ShareKitAvatarPrimitive>>* lastFaces;
@property(nonatomic, assign) NSInteger lastTotalNumber;
@property(nonatomic, assign) NSUInteger updateWithViewsCallCount;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FAKE_FACE_PILE_CONSUMER_H_
