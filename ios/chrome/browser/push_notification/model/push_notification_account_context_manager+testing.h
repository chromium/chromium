// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_TESTING_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_TESTING_H_

@interface PushNotificationAccountContextManager (Testing)

// Returns the number of times `gaiaID` has been signed into Chrome across
// Profiles.
- (NSUInteger)registrationCountForAccount:(const std::string&)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_ACCOUNT_CONTEXT_MANAGER_TESTING_H_
