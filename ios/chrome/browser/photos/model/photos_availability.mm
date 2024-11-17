// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/model/photos_availability.h"

#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/photos/model/photos_policy.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "url/gurl.h"

bool IsSaveToPhotosAvailable(ProfileIOS* profile) {
  CHECK(profile);

  // Check flag.
  if (!base::FeatureList::IsEnabled(kIOSSaveToPhotos)) {
    return false;
  }

  // Check policy.
  if (profile->GetPrefs()->GetInteger(
          prefs::kIosSaveToPhotosContextMenuPolicySettings) ==
      static_cast<int>(SaveToPhotosPolicySettings::kDisabled)) {
    return false;
  }

  // Check incognito.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  // Check PhotosService is available.
  PhotosService* photos_service = PhotosServiceFactory::GetForProfile(profile);
  if (!photos_service || !photos_service->IsAvailable()) {
    return false;
  }

  // Check user is signed in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;
}
