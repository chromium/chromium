// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PHOTOS_PHOTOS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PHOTOS_PHOTOS_API_H_

#import <memory>

class PhotosService;
@class PhotosServiceConfiguration;

namespace ios {
namespace provider {

// Creates a new instance of PhotosService.
std::unique_ptr<PhotosService> CreatePhotosService(
    PhotosServiceConfiguration* configuration);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PHOTOS_PHOTOS_API_H_
