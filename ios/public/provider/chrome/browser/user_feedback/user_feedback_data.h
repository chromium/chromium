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

// Stores a formatted string representation of the URL that the user was
// viewing. May be nil if the current active tab does show a valid page
// (e.g. the user was viewing the tab switcher, ...).
@property(nonatomic, strong) NSString* currentPageDisplayURL;

// Stores a screenshot of the application suitable for attaching to the
// user generated report.
@property(nonatomic, strong) UIImage* currentPageScreenshot;

// Stores the user name of the account being synchronized. Returns nil
// if sync is not enabled or the current active tab is in Incognito mode.
@property(nonatomic, strong) NSString* currentPageSyncedUserName;

// Stores additional product specific data to be attached to the user
// generated report.
@property(nonatomic, strong)
    NSDictionary<NSString*, NSString*>* productSpecificData;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_DATA_H_
