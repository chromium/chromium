// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration object for the ShareKit FacePile API.
@interface ShareKitFacePileConfiguration : NSObject

// Shared group ID.
@property(nonatomic, copy) NSString* collabID;

// The background color for the face pile when it is not empty.
@property(nonatomic, strong) UIColor* backgroundColor;

// Whether the face pile should be visible when the group is empty (not shared
// or shared with no members).
@property(nonatomic, assign) BOOL showsEmptyState;

// The preferred size in points for the avatar icons.
@property(nonatomic, assign) CGFloat avatarSize;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_
