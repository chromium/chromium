// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_

#import <UIKit/UIKit.h>

class GURL;

// Configuration object for joining a shared group.
@interface ShareKitJoinConfiguration : NSObject

// The base view controller on which the join flow will be presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The URL used to join the group, containing the collab ID and the token.
@property(nonatomic, assign) GURL URL;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_
