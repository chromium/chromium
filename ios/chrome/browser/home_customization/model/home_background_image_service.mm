// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"

#import "base/barrier_closure.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/themes/ntp_background_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {
// The filtering label to use for the default collection images.
const std::string kDefaultFilteringLabel = "default_chrome_ios_ntp";
}  // namespace

HomeBackgroundImageService::HomeBackgroundImageService(
    NtpBackgroundService* ntp_background_service)
    : ntp_background_service_(ntp_background_service) {
  ntp_background_service_->AddObserver(this);
}

HomeBackgroundImageService::~HomeBackgroundImageService() = default;

void HomeBackgroundImageService::FetchDefaultCollectionImages(
    CollectionsImagesCallback callback) {
  FetchCollectionsImagesInternal(std::move(callback), kDefaultFilteringLabel);
}

void HomeBackgroundImageService::FetchCollectionsImages(
    CollectionsImagesCallback callback) {
  FetchCollectionsImagesInternal(std::move(callback));
}

void HomeBackgroundImageService::FetchCollectionsImagesInternal(
    CollectionsImagesCallback callback,
    const std::string& filtering_label) {
  // If a request is already in progress, drop the new request.
  if (collections_images_callback_) {
    return;
  }

  collections_images_callback_ = std::move(callback);

  // If a request is currently in progress, drop the new request.
  if (all_images_received_barrier_) {
    return;
  }

  // Clear the collections images to start fresh.
  collections_images_.clear();

  // If a filtering label is provided, use it to fetch the collection info.
  if (!filtering_label.empty()) {
    ntp_background_service_->FetchCollectionInfo(filtering_label);
    return;
  }
  ntp_background_service_->FetchCollectionInfo();
}

void HomeBackgroundImageService::OnCollectionImageInfoReceived(
    size_t index,
    const std::string& collection_name,
    const std::vector<CollectionImage>& collection_images,
    ErrorType error_type) {
  if (error_type == ErrorType::NONE) {
    collections_images_[index] =
        std::make_tuple(collection_name, collection_images);
  }

  base::UmaHistogramEnumeration(
      "IOS.HomeCustomization.Background.Gallery.CollectionImageFetchError",
      error_type);

  // The `BarrierClosure` must be run regardless of the error type to ensure
  // that it is run `collection_count` times before the
  // `OnAllCollectionImagesReceived` callback can be run.
  all_images_received_barrier_.Run();
}

void HomeBackgroundImageService::OnAllCollectionImagesReceived() {
  // Reset the BarrierClosure.
  all_images_received_barrier_.Reset();

  if (!collections_images_callback_) {
    return;
  }

  // Remove any collections that are empty. This can happen if one of the
  // collection requests failed to fetch images due to network issues or
  // server errors, leaving empty slots in the pre-allocated vector.
  collections_images_.erase(
      std::remove_if(collections_images_.begin(), collections_images_.end(),
                     [](const auto& map) { return std::get<0>(map).empty(); }),
      collections_images_.end());

  std::move(collections_images_callback_).Run(collections_images_);
}

#pragma mark - NtpBackgroundServiceObserving

void HomeBackgroundImageService::OnCollectionInfoAvailable() {
  const std::vector<CollectionInfo>& collection_infos =
      ntp_background_service_->collection_info();

  base::UmaHistogramEnumeration(
      "IOS.HomeCustomization.Background.Gallery.CollectionInfoFetchError",
      ntp_background_service_->collection_error_info().error_type);

  if (collection_infos.empty()) {
    OnAllCollectionImagesReceived();
    return;
  }

  const size_t collection_count = collection_infos.size();

  // Pre-allocate the vector with the correct size to maintain order.
  collections_images_.resize(collection_count);

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback and returning the data. The
  // BarrierClosure will wait until the `OnAllCollectionImagesReceived` callback
  // is itself run `collection_count` times.
  all_images_received_barrier_ = base::BarrierClosure(
      collection_count,
      base::BindOnce(&HomeBackgroundImageService::OnAllCollectionImagesReceived,
                     weak_ptr_factory_.GetWeakPtr()));

  for (size_t i = 0; i < collection_infos.size(); ++i) {
    const CollectionInfo& collection_info = collection_infos[i];
    ntp_background_service_->FetchCollectionImageInfo(
        collection_info.collection_id,
        base::BindOnce(
            &HomeBackgroundImageService::OnCollectionImageInfoReceived,
            weak_ptr_factory_.GetWeakPtr(), /*index=*/i,
            collection_info.collection_name));
  }
}

void HomeBackgroundImageService::OnCollectionImagesAvailable() {}
void HomeBackgroundImageService::OnNextCollectionImageAvailable() {}
void HomeBackgroundImageService::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}
