// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Returns whether the Save to Photos entry point can be presented for a given
// profile.
bool IsSaveToPhotosAvailable(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_AVAILABILITY_H_
