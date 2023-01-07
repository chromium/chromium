// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"

@protocol ApplicationCommands;

// Configuration object used by the User Feedback view controller.
@interface UserFeedbackConfiguration : NSObject

// UserFeedbackData containing data to attach to the user generated report.
@property(nonatomic, strong) UserFeedbackData* data;

// SingleSignOnService used by the User Feedback view controller.
@property(nonatomic, weak) id<SingleSignOnService> singleSignOnService;

// ApplicationCommands used by the User Feedback view controller.
@property(nonatomic, weak) id<ApplicationCommands> handler;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_CONFIGURATION_H_
