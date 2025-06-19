// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "components/data_sharing/public/group_data.h"

// Configuration object to create a FacePileView.
@interface FacePileConfiguration : NSObject

// The group with which this face pile is associated.
@property(nonatomic, assign) data_sharing::GroupId groupID;

// The background color for the face pile, visible in gaps and as an outer
// stroke.
@property(nonatomic, strong) UIColor* backgroundColor;

// Whether the face pile should be visible when the group is empty.
@property(nonatomic, assign) BOOL showsEmptyState;

// The preferred size in points for the avatar icons.
@property(nonatomic, assign) CGFloat avatarSize;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_CONFIGURATION_H_
