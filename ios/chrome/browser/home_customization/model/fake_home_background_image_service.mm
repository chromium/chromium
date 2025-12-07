// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/fake_home_background_image_service.h"

FakeHomeBackgroundImageService::FakeHomeBackgroundImageService(
    NtpBackgroundService* ntp_background_service)
    : HomeBackgroundImageService(ntp_background_service) {}

FakeHomeBackgroundImageService::~FakeHomeBackgroundImageService() = default;

void FakeHomeBackgroundImageService::FetchCollectionsImages(
    CollectionsImagesCallback callback) {
  std::move(callback).Run(collection_data_);
}
void FakeHomeBackgroundImageService::FetchDefaultCollectionImages(
    CollectionsImagesCallback callback) {
  std::move(callback).Run(default_collection_data_);
}

void FakeHomeBackgroundImageService::SetCollectionData(
    HomeBackgroundImageService::CollectionImageMap collection_data) {
  collection_data_ = collection_data;
}
void FakeHomeBackgroundImageService::SetDefaultCollectionData(
    HomeBackgroundImageService::CollectionImageMap default_collection_data) {
  default_collection_data_ = default_collection_data;
}
