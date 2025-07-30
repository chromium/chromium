// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

bool IsPersonalProfile(ProfileIOS* profile) {
  return profile->GetProfileName() == GetApplicationContext()
                                          ->GetProfileManager()
                                          ->GetProfileAttributesStorage()
                                          ->GetPersonalProfileName();
}
