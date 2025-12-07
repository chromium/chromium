// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_HOME_BACKGROUND_IMAGE_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_HOME_BACKGROUND_IMAGE_SERVICE_H_

#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"

class NtpBackgroundService;

/// Fake `HomeBackgroundImageService` for use in tests. Allows easy
/// configuration of returned data. The `NtpBackgroundService` is required for
/// initializing `HomeBackgroundImageService`, but not used.
class FakeHomeBackgroundImageService : public HomeBackgroundImageService {
 public:
  explicit FakeHomeBackgroundImageService(
      NtpBackgroundService* ntp_background_service);
  ~FakeHomeBackgroundImageService() override;

  // HomeBackgroundImageService:
  void FetchCollectionsImages(CollectionsImagesCallback callback) override;
  void FetchDefaultCollectionImages(
      CollectionsImagesCallback callback) override;

  // Sets the data to be returned in future calls to `FetchCollectionsImages`.
  void SetCollectionData(
      HomeBackgroundImageService::CollectionImageMap collection_data);

  // Sets the data to be returned in future calls to
  // `FetchDefaultCollectionImages`.
  void SetDefaultCollectionData(
      HomeBackgroundImageService::CollectionImageMap default_collection_data);

  // Stored the data to be returned in future calls to `FetchCollectionsImages`.
  HomeBackgroundImageService::CollectionImageMap collection_data_;

  // Stored the data to be returned in future calls to
  // `FetchDefaultCollectionImages`.
  HomeBackgroundImageService::CollectionImageMap default_collection_data_;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_FAKE_HOME_BACKGROUND_IMAGE_SERVICE_H_
