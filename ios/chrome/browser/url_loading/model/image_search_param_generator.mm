// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/image_search_param_generator.h"

#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/search_engines/template_url_service.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_util.h"

namespace {

// Resizes `image` and encodes it as JPEG. Must be called on a background
// thread. Returns the encoded image data, or nil if the image is empty.
NSData* ResizeAndEncodeImage(UIImage* image) {
  gfx::Image gfx_image(image);
  if (gfx_image.IsEmpty()) {
    return nil;
  }
  UIImage* resized_image =
      gfx::ResizedImageForSearchByImage(gfx_image).ToUIImage();
  return UIImageJPEGRepresentation(resized_image, 1.0);
}

// Decodes `data` into a UIImage, resizes it, and encodes it as JPEG. Must
// be called on a background thread. If the image cannot be decoded or does
// not need resizing, returns the original `data` unchanged.
NSData* DecodeResizeAndEncodeImageData(NSData* data) {
  UIImage* image = [UIImage imageWithData:data];
  gfx::Image gfx_image(image);
  if (gfx_image.IsEmpty()) {
    return data;
  }
  UIImage* resized_image =
      gfx::ResizedImageForSearchByImage(gfx_image).ToUIImage();
  if ([image isEqual:resized_image]) {
    return data;
  }
  return UIImageJPEGRepresentation(resized_image, 1.0);
}

}  // namespace

void ImageSearchParamGenerator::PrepareImageDataAsync(
    UIImage* image,
    ImageDataCallback callback) {
  __block NSData* resultData = nil;
  __block ImageDataCallback completionCallback = std::move(callback);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(^{
        resultData = ResizeAndEncodeImage(image);
      }),
      base::BindOnce(^{
        std::move(completionCallback).Run(resultData);
      }));
}

void ImageSearchParamGenerator::PrepareImageDataFromDataAsync(
    NSData* data,
    ImageDataCallback callback) {
  __block NSData* resultData = nil;
  __block ImageDataCallback completionCallback = std::move(callback);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(^{
        resultData = DecodeResizeAndEncodeImageData(data);
      }),
      base::BindOnce(^{
        std::move(completionCallback).Run(resultData);
      }));
}

web::NavigationManager::WebLoadParams
ImageSearchParamGenerator::LoadParamsForResizedImageData(
    NSData* data,
    const GURL& url,
    TemplateURLService* template_url_service) {
  std::string byte_string;
  if (data) {
    char const* bytes = reinterpret_cast<const char*>([data bytes]);
    byte_string.assign(bytes, [data length]);
  }

  const TemplateURL* default_url =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_url);
  DCHECK(!default_url->image_url().empty());
  DCHECK(default_url->image_url_ref().IsValid(
      template_url_service->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_args(u"");
  if (!url.is_empty()) {
    search_args.image_url = url;
  }
  search_args.image_thumbnail_content = byte_string;

  // Generate the URL and populate `post_content` with the content type and
  // HTTP body for the request.
  TemplateURLRef::PostContent post_content;
  GURL result(default_url->image_url_ref().ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));
  web::NavigationManager::WebLoadParams web_load_params =
      web_navigation_util::CreateWebLoadParams(
          result, ui::PAGE_TRANSITION_TYPED, &post_content);

  return web_load_params;
}

web::NavigationManager::WebLoadParams
ImageSearchParamGenerator::LoadParamsForImageData(
    NSData* data,
    const GURL& url,
    TemplateURLService* template_url_service) {
  NSData* image_data = data;
  UIImage* image = [UIImage imageWithData:image_data];
  gfx::Image gfx_image(image);
  if (gfx_image.IsEmpty()) {
    return LoadParamsForResizedImageData(image_data, url, template_url_service);
  }
  UIImage* resized_image =
      gfx::ResizedImageForSearchByImage(gfx_image).ToUIImage();
  if (![image isEqual:resized_image]) {
    image_data = UIImageJPEGRepresentation(resized_image, 1.0);
  }
  return LoadParamsForResizedImageData(image_data, url, template_url_service);
}

web::NavigationManager::WebLoadParams
ImageSearchParamGenerator::LoadParamsForImage(
    UIImage* image,
    TemplateURLService* template_url_service) {
  gfx::Image gfx_image(image);
  if (gfx_image.IsEmpty()) {
    return LoadParamsForResizedImageData(nil, GURL(), template_url_service);
  }
  UIImage* resized_image =
      gfx::ResizedImageForSearchByImage(gfx_image).ToUIImage();
  NSData* data = UIImageJPEGRepresentation(resized_image, 1.0);
  return LoadParamsForResizedImageData(data, GURL(), template_url_service);
}
