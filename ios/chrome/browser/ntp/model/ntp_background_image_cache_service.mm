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
}

UIImage* NTPBackgroundImageCacheService::GetCachedBackgroundImage() {
  return [background_image_cache_ objectForKey:@(kCacheKey)];
}

void NTPBackgroundImageCacheService::SetCachedBackgroundImage(UIImage* image) {
  if (!image) {
    background_image_cache_ = nil;
    return;
  }
  background_image_cache_ = [[NSCache alloc] init];
  [background_image_cache_ setObject:image forKey:@(kCacheKey)];
}
