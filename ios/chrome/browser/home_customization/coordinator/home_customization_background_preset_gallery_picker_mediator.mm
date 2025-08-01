// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_preset_gallery_picker_mediator.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "ios/chrome/browser/home_customization/model/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration_item.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_consumer.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

@interface HomeCustomizationBackgroundPresetGalleryPickerMediator () {
  // The image fetcher used to download individual background preset images.
  raw_ptr<image_fetcher::ImageFetcher> _imageFetcher;
  // The service that provides the background images.
  raw_ptr<HomeBackgroundImageService> _homeBackgroundImageService;
  // Used to get and observe the background state.
  raw_ptr<HomeBackgroundCustomizationService> _backgroundCustomizationService;
}

@end

@implementation HomeCustomizationBackgroundPresetGalleryPickerMediator

- (instancetype)initWithImageFetcherService:
                    (image_fetcher::ImageFetcherService*)imageFetcherService
                 homeBackgroundImageService:
                     (HomeBackgroundImageService*)homeBackgroundImageService
             backgroundCustomizationService:
                 (HomeBackgroundCustomizationService*)
                     backgroundCustomizationService {
  self = [super init];
  if (self) {
    _imageFetcher = imageFetcherService->GetImageFetcher(
        image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
    _homeBackgroundImageService = homeBackgroundImageService;
    _backgroundCustomizationService = backgroundCustomizationService;
  }
  return self;
}

- (void)loadBackgroundConfigurations {
  __weak HomeCustomizationBackgroundPresetGalleryPickerMediator* weakself =
      self;
  _homeBackgroundImageService->FetchCollectionsImages(
      base::BindOnce(^(const HomeBackgroundImageService::CollectionImageMap&
                           collectionMapParam) {
        if (!collectionMapParam.empty()) {
          [weakself onCollectionDataReceived:collectionMapParam];
        }
      }));
}

#pragma mark - HomeCustomizationBackgroundPresetGalleryPickerMutator

- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:
                                               (void (^)(UIImage*))completion {
  CHECK(!thumbnailURL.is_empty());
  CHECK(thumbnailURL.is_valid());

  _imageFetcher->FetchImage(
      thumbnailURL,
      base::BindOnce(^(const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
        if (!image.IsEmpty()) {
          UIImage* uiImage = image.ToUIImage();
          if (completion) {
            completion(uiImage);
          }
        }
      }),
      // TODO (crbug.com/417234848): Add annotation.
      image_fetcher::ImageFetcherParams(NO_TRAFFIC_ANNOTATION_YET, "Test"));
}

#pragma mark - Private

// Callback function that is called when the collection images are fetched. This
// will then create BackgroundCollectionConfiguration objects and send them to
// the consumer.
- (void)onCollectionDataReceived:
    (HomeBackgroundImageService::CollectionImageMap)collectionMap {
  NSMutableArray<BackgroundCollectionConfiguration*>* collectionConfigurations =
      [NSMutableArray array];

  std::optional<sync_pb::NtpCustomBackground> background =
      _backgroundCustomizationService->GetCurrentCustomBackground();

  NSString* selectedBackgroundId = nil;

  for (const auto& [collectionName, collectionImages] : collectionMap) {
    // Create a new section for the collection.
    BackgroundCollectionConfiguration* section =
        [[BackgroundCollectionConfiguration alloc] init];
    section.collectionName = base::SysUTF8ToNSString(collectionName);
    NSMutableArray<BackgroundCustomizationConfigurationItem*>*
        imageConfigurations = [[NSMutableArray alloc] init];
    for (const auto& image : collectionImages) {
      BackgroundCustomizationConfigurationItem* config =
          [[BackgroundCustomizationConfigurationItem alloc]
              initWithCollectionImage:image];
      [imageConfigurations addObject:config];

      if (background && image.image_url == background->url()) {
        selectedBackgroundId = config.configurationID;
      }
    }
    section.configurations = [NSArray arrayWithArray:imageConfigurations];
    [collectionConfigurations addObject:section];
  }

  [_consumer setBackgroundCollectionConfigurations:collectionConfigurations
                              selectedBackgroundId:selectedBackgroundId];
}

@end
