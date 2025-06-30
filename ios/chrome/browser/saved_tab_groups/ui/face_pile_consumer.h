// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol ShareKitAvatarPrimitive;

// Protocol for the FacePileView to update its display.
@protocol FacePileConsumer <NSObject>

// Sets whether the FacePileView should display text when there are no faces.
- (void)setSharedButtonWhenEmpty:(BOOL)showsShareButtonWhenEmpty;

// Sets the background color for the face pile, visible in gaps and as an outer
// stroke.
- (void)setFacePileBackgroundColor:(UIColor*)backgroundColor;

// Sets the size of avatar faces, in points.
- (void)setAvatarSize:(CGFloat)avatarSize;

// Updates the FacePileView with a new set of faces and the total
// member count.
- (void)updateWithFaces:(NSArray<id<ShareKitAvatarPrimitive>>*)faces
            totalNumber:(NSInteger)totalNumber;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_CONSUMER_H_
