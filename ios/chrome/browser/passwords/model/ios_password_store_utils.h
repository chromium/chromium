// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_UTILS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_UTILS_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Query the password stores and reports multiple metrics. The actual reporting
// is delayed by 30 seconds, to ensure it doesn't happen during the "hot phase"
// of Chrome startup.
void DelayReportingPasswordStoreMetrics(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_UTILS_H_
