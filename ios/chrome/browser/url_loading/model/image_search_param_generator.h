// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"

class GURL;
class TemplateURLService;

class ImageSearchParamGenerator {
 public:
  // Callback that receives the resized and encoded image data.
  using ImageDataCallback = base::OnceCallback<void(NSData*)>;

  // Resizes and JPEG-encodes `image` on a background thread, then invokes
  // `callback` on the main thread with the resulting data (or nil if the
  // image is empty). Callers should use LoadParamsForResizedImageData() in
  // the callback to build WebLoadParams.
  static void PrepareImageDataAsync(UIImage* image, ImageDataCallback callback);

  // Decodes `data` into a UIImage, resizes it, and JPEG-encodes on a
  // background thread, then invokes `callback` on the main thread with
  // the resulting data. If the image cannot be decoded, returns the
  // original data unchanged.
  static void PrepareImageDataFromDataAsync(NSData* data,
                                            ImageDataCallback callback);

  // Creates loading parameters from already-resized image `data` and
  // `url`. This is synchronous and must be called on the main thread.
  static web::NavigationManager::WebLoadParams LoadParamsForResizedImageData(
      NSData* data,
      const GURL& url,
      TemplateURLService* template_url_service);

  // Synchronous versions that perform image resizing and JPEG encoding on
  // the calling thread.

  // Create loading parameters using the given `data`, which should represent
  // an image and `url`, the web url the image came from. If the image data
  // didn't come from a url, use an empty GURL to indicate that.
  static web::NavigationManager::WebLoadParams LoadParamsForImageData(
      NSData* data,
      const GURL& url,
      TemplateURLService* template_url_service);

  // Create loading parameters using the given `image`.
  static web::NavigationManager::WebLoadParams LoadParamsForImage(
      UIImage* image,
      TemplateURLService* template_url_service);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_
