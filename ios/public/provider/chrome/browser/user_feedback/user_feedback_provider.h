// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"

@protocol ApplicationCommands;

// This data source object is used to obtain initial data to populate the fields
// on the User Feedback form.
@protocol UserFeedbackDataSource<NSObject>

// Returns whether user was viewing a tab in Incognito mode.
- (BOOL)currentPageIsIncognito;

// Returns a formatted string representation of the URL that the user was
// viewing. May return nil if the tab viewed does not have a valid URL to be
// shown (e.g. user was viewing stack view controller).
- (NSString*)currentPageDisplayURL;

// Returns a screenshot of the application suitable for attaching to problem
// reports submitted via Send Feedback UI.
// Screenshot is taken lazily only when needed.
- (UIImage*)currentPageScreenshot;

// Returns the username of the account being synced.
// Returns nil if sync is not enabled or user is in incognito mode.
- (NSString*)currentPageSyncedUserName;

@end

// UserFeedbackProvider allows embedders to provide functionality to collect
// and upload feedback reports from the user.
class UserFeedbackProvider {
 public:
  UserFeedbackProvider();
  virtual ~UserFeedbackProvider();
  // Returns true if user feedback is enabled.
  virtual bool IsUserFeedbackEnabled();
  // Returns view controller to present to the user to collect their feedback.
  // |data_source| provides the information to initialize the view controller
  // and |dispatcher| is an object from the embedder that can perform operations
  // on behalf of the UserFeedbackProvider.
  virtual UIViewController* CreateViewController(
      id<UserFeedbackDataSource> data_source,
      id<ApplicationCommands> dispatcher);
  // Uploads collected feedback reports.
  virtual void Synchronize();

 private:
  DISALLOW_COPY_AND_ASSIGN(UserFeedbackProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_
