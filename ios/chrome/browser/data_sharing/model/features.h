// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_FEATURES_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// Whether the Shared Tab Groups feature is enabled and a user can join to an
// existing shared group.
bool IsSharedTabGroupsJoinEnabled(ProfileIOS* profile);

// Whether the Shared Tab Groups feature is enabled and a user can create a new
// shared group.
bool IsSharedTabGroupsCreateEnabled(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_FEATURES_H_
