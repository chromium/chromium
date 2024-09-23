// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_DATA_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_DATA_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

// Object storing the data about the application to include with the
// user generated feedback.
@interface UserFeedbackData : NSObject

// Stores the origin of the request to generate an user feedback.
@property(nonatomic, assign) UserFeedbackSender origin;

// Stores whether the current active tab is in Incognito mode.
@property(nonatomic, assign) BOOL currentPageIsIncognito;

// Stores the member role for a Family Link user, otherwise this
// property is not populated.
@property(nonatomic, strong) NSString* familyMemberRole;

// Stores a screenshot of the application suitable for attaching to the
// user generated report.
@property(nonatomic, strong) UIImage* currentPageScreenshot;

// Stores additional product specific data to be attached to the user
// generated report.
@property(nonatomic, strong)
    NSDictionary<NSString*, NSString*>* productSpecificData;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_DATA_H_
