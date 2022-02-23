// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

@protocol ApplicationCommands;

// This data source object is used to obtain initial data to populate the fields
// on the User Feedback form.
// TODO(crbug.com/1117041): Rename this protocol to something more specific to
// currentPageScreenshot since that might be the only method remaining.
@protocol UserFeedbackDataSource<NSObject>

// Returns whether user was viewing a tab in Incognito mode.
// TODO(crbug.com/1117041): Move this into a UserFeedback config object.
- (BOOL)currentPageIsIncognito;

// Returns a formatted string representation of the URL that the user was
// viewing. May return nil if the tab viewed does not have a valid URL to be
// shown (e.g. user was viewing stack view controller).
// TODO(crbug.com/1117041): Move this into a UserFeedback config object.
- (NSString*)currentPageDisplayURL;

// Returns a screenshot of the application suitable for attaching to problem
// reports submitted via Send Feedback UI.
// Screenshot is taken lazily only when needed.
- (UIImage*)currentPageScreenshot;

// Returns the username of the account being synced.
// Returns nil if sync is not enabled or user is in incognito mode.
// TODO(crbug.com/1117041): Move this into a UserFeedback config object.
- (NSString*)currentPageSyncedUserName;

// Returns the additional product specific data to be sent in the Send Feedback
// report.
// TODO(crbug.com/1117041): Move this into a UserFeedback config object.
- (NSDictionary<NSString*, NSString*>*)specificProductData;

@end

// UserFeedbackProvider allows embedders to provide functionality to collect
// and upload feedback reports from the user.
class UserFeedbackProvider {
 public:
  UserFeedbackProvider();

  UserFeedbackProvider(const UserFeedbackProvider&) = delete;
  UserFeedbackProvider& operator=(const UserFeedbackProvider&) = delete;

  virtual ~UserFeedbackProvider();
  // Returns true if user feedback is enabled.
  virtual bool IsUserFeedbackEnabled();
  // Returns view controller to present to the user to collect their feedback.
  // |data_source| provides the information to initialize the view controller
  // and |dispatcher| is an object from the embedder that can perform operations
  // on behalf of the UserFeedbackProvider.
  // TODO(crbug.com/1117041): Send a configuration object instead of these
  // parameters.
  virtual UIViewController* CreateViewController(
      id<UserFeedbackDataSource> data_source,
      id<ApplicationCommands> handler,
      UserFeedbackSender sender);
  // Uploads collected feedback reports.
  virtual void Synchronize();
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_PROVIDER_H_
