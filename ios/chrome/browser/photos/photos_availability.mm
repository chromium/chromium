// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/photos/photos_availability.h"

#import "base/feature_list.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/photos/photos_service.h"
#import "ios/chrome/browser/photos/photos_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "url/gurl.h"

bool IsSaveToPhotosAvailableForImageUrl(const GURL& image_url,
                                        ChromeBrowserState* browser_state) {
  CHECK(browser_state);

  // Check flag.
  if (!base::FeatureList::IsEnabled(kIOSSaveToPhotos)) {
    return false;
  }

  // Check incognito.
  if (browser_state->IsOffTheRecord()) {
    return false;
  }

  // Check PhotosService is available.
  PhotosService* photos_service =
      PhotosServiceFactory::GetForBrowserState(browser_state);
  if (!photos_service || !photos_service->IsAvailable()) {
    return false;
  }

  // Check user is signed in.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  if (!authentication_service || !authentication_service->HasPrimaryIdentity(
                                     signin::ConsentLevel::kSignin)) {
    return false;
  }

  return true;
}
