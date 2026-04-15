// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/ntp_background_image_cache_service.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

namespace {
// Key used to lookup the image in the NSCache.
constexpr int kCacheKey = 1;
}  // namespace

NTPBackgroundImageCacheService::NTPBackgroundImageCacheService(
    HomeBackgroundCustomizationService* background_customization_service)
    : background_image_cache_(nil) {
  if (background_customization_service) {
    background_customization_service_observation_.Observe(
        background_customization_service);
  }
}

NTPBackgroundImageCacheService::~NTPBackgroundImageCacheService() {}

void NTPBackgroundImageCacheService::Shutdown() {
  background_customization_service_observation_.Reset();
}

void NTPBackgroundImageCacheService::OnBackgroundChanged() {
  background_image_cache_ = nil;
  cached_original_image_size_ = CGSizeZero;
}

UIImage* NTPBackgroundImageCacheService::GetCachedBackgroundImage() {
  return [background_image_cache_ objectForKey:@(kCacheKey)];
}

CGSize NTPBackgroundImageCacheService::GetCachedOriginalImageSize() {
  return cached_original_image_size_;
}

void NTPBackgroundImageCacheService::SetCachedBackgroundImage(
    UIImage* image,
    CGSize original_image_size) {
  if (!image) {
    background_image_cache_ = nil;
    cached_original_image_size_ = CGSizeZero;
    return;
  }
  background_image_cache_ = [[NSCache alloc] init];
  [background_image_cache_ setObject:image forKey:@(kCacheKey)];
  cached_original_image_size_ = original_image_size;
}
