// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google/model/google_logo_service.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Cache directory for doodle.
base::FilePath DoodleDirectory() {
  base::FilePath cache;
  const bool success = base::PathService::Get(base::DIR_CACHE, &cache);
  DCHECK(success) << "Failed to get cache dir path.";
  return cache.Append(FILE_PATH_LITERAL("Chromium"))
      .Append(FILE_PATH_LITERAL("Doodle"));
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
