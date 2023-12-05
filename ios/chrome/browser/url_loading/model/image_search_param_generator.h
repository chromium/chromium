// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/web/model/web_navigation_util.h"

class GURL;
class TemplateURLService;

class ImageSearchParamGenerator {
 public:
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

 private:
  static web::NavigationManager::WebLoadParams LoadParamsForResizedImageData(
      NSData* data,
      const GURL& url,
      TemplateURLService* template_url_service);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_IMAGE_SEARCH_PARAM_GENERATOR_H_
