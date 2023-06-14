// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_ICON_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_ICON_H_

#import <UIKit/UIKit.h>

enum class SetUpListItemType;
namespace base {
class TimeDelta;
}

// A view which contains an icon for a Set Up List item.
@interface SetUpListItemIcon : UIView

// Instantiates a `SetUpListItemItemIcon` for the given `type` with the
// given `complete` state, whether to configure with a `compactLayout`, and
// whether it should be placed `inSquare` container.
- (instancetype)initWithType:(SetUpListItemType)type
                    complete:(BOOL)complete
               compactLayout:(BOOL)compactLayout
                    inSquare:(BOOL)inSquare;

// Plays the "sparkle" animation with the given `duration`, after the given
// `delay`.
- (void)playSparkleWithDuration:(base::TimeDelta)duration
                          delay:(base::TimeDelta)delay;

// Marks this item as complete by hiding the type-specific icon and showing a
// checkmark. When called as part of an animation, the type-specific icon and
// the checkmark swap via a rotation and crossfade animation.
- (void)markComplete;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_ICON_H_
