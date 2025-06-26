// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_VIEW_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_consumer.h"

// A view that displays a "pile" of faces, typically user avatars.
@interface FacePileView : UIView <FacePileConsumer>

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The width that the FacePileView will take.
@property(nonatomic, readonly) CGFloat optimalWidth;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_VIEW_H_
