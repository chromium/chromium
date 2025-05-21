// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/themes/ntp_background_data.h"
#import "components/themes/ntp_background_service_observer.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"

class NtpBackgroundService;

// HomeBackgroundImageService is a model that manages multiple NTP images for
// the background.
class HomeBackgroundImageService : public KeyedService,
                                   public NtpBackgroundServiceObserver {
 public:
  explicit HomeBackgroundImageService(
      NtpBackgroundService* ntp_background_service);
  HomeBackgroundImageService(const HomeBackgroundImageService&) = delete;
  HomeBackgroundImageService& operator=(const HomeBackgroundImageService&) =
      delete;
  ~HomeBackgroundImageService() override;

  // NtpBackgroundServiceObserver:
  void OnCollectionInfoAvailable() override;
  void OnCollectionImagesAvailable() override;
  void OnNextCollectionImageAvailable() override;
  void OnNtpBackgroundServiceShuttingDown() override;

  // Storage for collections and their images.
  using CollectionImageMap =
      std::vector<std::tuple<std::string, std::vector<CollectionImage>>>;
  // Callback type for fetching collections and their images, invoked with a
  // vector of tuples of collection name and collection images.
  using CollectionsImagesCallback =
      base::OnceCallback<void(const CollectionImageMap&)>;
  // Requests an asynchronous fetch of all collections and their images. This
  // calls the `NtpBackgroundService` to fetch the collections images and then
  // returns the data via the `CollectionsImagesCallback`. Requests that are
  // made while an asynchronous fetch is in progress will be dropped until the
  // currently active loader completes.
  void FetchCollectionsImages(CollectionsImagesCallback callback);

 private:
  // Callback for when collection images info is received.
  void OnCollectionImageInfoReceived(
      const std::string& collection_name,
      const std::vector<CollectionImage>& collection_images,
      ErrorType error_type);

  // Callback for when all collections and their images have been received.
  void OnAllCollectionImagesReceived();

  CollectionsImagesCallback collections_images_callback_;
  CollectionImageMap collections_images_;
  raw_ptr<NtpBackgroundService> ntp_background_service_;
  base::RepeatingClosure all_images_received_barrier_;
  base::WeakPtrFactory<HomeBackgroundImageService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_IMAGE_SERVICE_H_
