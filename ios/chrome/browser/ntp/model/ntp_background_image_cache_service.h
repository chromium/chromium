// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_H_

#import <CoreGraphics/CoreGraphics.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"

@class NSCache<KeyType, ObjectType>;
@class NSNumber;
@class UIImage;

// Service for caching the NTP background image to allow immediate display
// on NTP creation.
class NTPBackgroundImageCacheService
    : public KeyedService,
      public HomeBackgroundCustomizationServiceObserver {
 public:
  explicit NTPBackgroundImageCacheService(
      HomeBackgroundCustomizationService* background_customization_service);

  NTPBackgroundImageCacheService(const NTPBackgroundImageCacheService&) =
      delete;
  NTPBackgroundImageCacheService& operator=(
      const NTPBackgroundImageCacheService&) = delete;

  ~NTPBackgroundImageCacheService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // HomeBackgroundCustomizationServiceObserver implementation.
  void OnBackgroundChanged() override;

  // Returns the cached background image, or nil if none is cached.
  UIImage* GetCachedBackgroundImage();

  // Returns the cached original image size, or CGSizeZero if none is cached.
  CGSize GetCachedOriginalImageSize();

  // Sets the cached background image and its original image size.
  void SetCachedBackgroundImage(UIImage* image, CGSize original_image_size);

 private:
  // Contains the cached background image.
  NSCache<NSNumber*, UIImage*>* background_image_cache_;

  // The original image size associated with the cached image.
  CGSize cached_original_image_size_ = CGSizeZero;

  // Observation of the HomeBackgroundCustomizationService.
  base::ScopedObservation<HomeBackgroundCustomizationService,
                          HomeBackgroundCustomizationServiceObserver>
      background_customization_service_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_NTP_BACKGROUND_IMAGE_CACHE_SERVICE_H_
