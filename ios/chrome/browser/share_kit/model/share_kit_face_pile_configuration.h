// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_

#import <Foundation/Foundation.h>

// Configuration object for the ShareKit FacePile API.
@interface ShareKitFacePileConfiguration : NSObject

// Shared group ID.
@property(nonatomic, copy) NSString* collabID;
// The completion block to be called when the share button is tapped. CollabID
// is the ID of the group that the user is invited to. IsSignedIn is YES if the
// user is signed in.
@property(nonatomic, copy) void (^completionBlock)
    (NSString* collabID, BOOL isSignedIn);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_FACE_PILE_CONFIGURATION_H_
