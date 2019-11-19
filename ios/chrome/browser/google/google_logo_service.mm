// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/google/google_logo_service.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

static NSArray* const kDoodleCacheDirectory = @[ @"Chromium", @"Doodle" ];

// Cache directory for doodle.
base::FilePath DoodleDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                       NSUserDomainMask, YES);
  NSString* path = [paths objectAtIndex:0];
  NSArray* path_components =
      [NSArray arrayWithObjects:path, kDoodleCacheDirectory[0],
                                kDoodleCacheDirectory[1], nil];
  return base::FilePath(
      base::SysNSStringToUTF8([NSString pathWithComponents:path_components]));
}

}  // namespace

GoogleLogoService::GoogleLogoService(
    TemplateURLService* template_url_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : LogoServiceImpl(
          DoodleDirectory(),
          identity_manager,
          template_url_service,
          image_fetcher::CreateIOSImageDecoder(),
          std::move(url_loader_factory),
          /*want_gray_logo_getter=*/base::BindRepeating([] { return false; })) {
}

GoogleLogoService::~GoogleLogoService() {}

void GoogleLogoService::SetCachedLogo(const search_provider_logos::Logo* logo) {
  if (logo) {
    if (cached_metadata_.fingerprint == logo->metadata.fingerprint) {
      return;
    }
    if (cached_image_.tryAllocPixels(logo->image.info())) {
      logo->image.readPixels(cached_image_.info(), cached_image_.getPixels(),
                             cached_image_.rowBytes(), 0, 0);
    }
    cached_metadata_ = logo->metadata;
  } else {
    cached_image_ = SkBitmap();
    cached_metadata_ = empty_metadata;
  }
}

search_provider_logos::Logo GoogleLogoService::GetCachedLogo() {
  search_provider_logos::Logo logo;
  logo.image = cached_image_;
  logo.metadata = cached_metadata_;
  return logo;
}
