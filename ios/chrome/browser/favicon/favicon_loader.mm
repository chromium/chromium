// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_loader.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "skia/ext/skia_utils_ios.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kFallbackIconDefaultTextColor = 0xAAAAAA;

// NetworkTrafficAnnotationTag for fetching favicon from a Google server.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("favicon_loader_get_large_icon", R"(
        semantics {
        sender: "FaviconLoader"
        description:
            "Sends a request to a Google server to retrieve the favicon bitmap."
        trigger:
            "A request can be sent if Chrome does not have a favicon."
        data: "Page URL and desired icon size."
        destination: GOOGLE_OWNED_SERVICE
        }
        policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification: "Not implemented."
        }
        )");
}  // namespace

FaviconLoader::FaviconLoader(favicon::LargeIconService* large_icon_service)
    : large_icon_service_(large_icon_service),
      favicon_cache_([[NSCache alloc] init]) {}
FaviconLoader::~FaviconLoader() {}

// TODO(pinkerton): How do we update the favicon if it's changed on the web?
// We can possibly just rely on this class being purged or the app being killed
// to reset it, but then how do we ensure the FaviconService is updated?
void FaviconLoader::FaviconForPageUrl(
    const GURL& page_url,
    float size_in_points,
    float min_size_in_points,
    bool fallback_to_google_server,  // retrieve favicon from Google Server if
                                     // GetLargeIconOrFallbackStyle() doesn't
                                     // return valid favicon.
    FaviconAttributesCompletionBlock faviconBlockHandler) {
  DCHECK(faviconBlockHandler);
  NSString* key =
      [NSString stringWithFormat:@"%d %@", (int)round(size_in_points),
                                 base::SysUTF8ToNSString(page_url.spec())];
  FaviconAttributes* value = [favicon_cache_ objectForKey:key];
  if (value) {
    faviconBlockHandler(value);
    return;
  }

  const CGFloat scale = UIScreen.mainScreen.scale;
  GURL block_page_url(page_url);
  auto favicon_block = ^(const favicon_base::LargeIconResult& result) {
    // GetLargeIconOrFallbackStyle() either returns a valid favicon (which can
    // be the default favicon) or fallback attributes.
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      // The favicon code assumes favicons are PNG-encoded.
      UIImage* favicon = [UIImage
          imageWithData:[NSData dataWithBytes:data->front() length:data->size()]
                  scale:scale];
      FaviconAttributes* attributes =
          [FaviconAttributes attributesWithImage:favicon];
      [favicon_cache_ setObject:attributes forKey:key];

      DCHECK(favicon.size.width <= size_in_points &&
             favicon.size.height <= size_in_points);
      faviconBlockHandler(attributes);
      return;
    } else if (fallback_to_google_server) {
      void (^favicon_loaded_from_server_block)(
          favicon_base::GoogleFaviconServerRequestStatus status) =
          ^(const favicon_base::GoogleFaviconServerRequestStatus status) {
            // Update the time when the icon was last requested - postpone thus
            // the automatic eviction of the favicon from the favicon database.
            large_icon_service_->TouchIconFromGoogleServer(block_page_url);

            // Favicon should be loaded to the db that backs LargeIconService
            // now.  Fetch it again. Even if the request was not successful, the
            // fallback style will be used.
            FaviconForPageUrl(
                block_page_url, size_in_points, min_size_in_points,
                /*continueToGoogleServer=*/false, faviconBlockHandler);
          };

      large_icon_service_
          ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
              block_page_url,
              /*may_page_url_be_private=*/true,
              /*should_trim_page_url_path=*/false, kTrafficAnnotation,
              base::BindRepeating(favicon_loaded_from_server_block));
      return;
    }

    // Did not get valid favicon back and are not attempting to retrieve one
    // from a Google Server.
    DCHECK(result.fallback_icon_style);
    FaviconAttributes* attributes = [FaviconAttributes
        attributesWithMonogram:base::SysUTF16ToNSString(
                                   favicon::GetFallbackIconText(block_page_url))
                     textColor:UIColorFromRGB(kFallbackIconDefaultTextColor)
               backgroundColor:UIColor.clearColor
        defaultBackgroundColor:result.fallback_icon_style->
                               is_default_background_color];

    [favicon_cache_ setObject:attributes forKey:key];
    faviconBlockHandler(attributes);
  };

  // First, synchronously return a fallback image.
  faviconBlockHandler([FaviconAttributes attributesWithDefaultImage]);

  // Now fetch the image synchronously.
  DCHECK(large_icon_service_);
  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      page_url, scale * min_size_in_points, scale * size_in_points,
      base::BindRepeating(favicon_block), &cancelable_task_tracker_);
}

void FaviconLoader::FaviconForIconUrl(
    const GURL& icon_url,
    float size_in_points,
    float min_size_in_points,
    FaviconAttributesCompletionBlock faviconBlockHandler) {
  DCHECK(faviconBlockHandler);
  NSString* key =
      [NSString stringWithFormat:@"%d %@", (int)round(size_in_points),
                                 base::SysUTF8ToNSString(icon_url.spec())];
  FaviconAttributes* value = [favicon_cache_ objectForKey:key];
  if (value) {
    faviconBlockHandler(value);
    return;
  }

  const CGFloat scale = UIScreen.mainScreen.scale;
  const CGFloat favicon_size_in_pixels = scale * size_in_points;
  const CGFloat min_favicon_size_in_pixels = scale * min_size_in_points;
  GURL block_icon_url(icon_url);
  auto favicon_block = ^(const favicon_base::LargeIconResult& result) {
    // GetLargeIconOrFallbackStyle() either returns a valid favicon (which can
    // be the default favicon) or fallback attributes.
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      // The favicon code assumes favicons are PNG-encoded.
      UIImage* favicon = [UIImage
          imageWithData:[NSData dataWithBytes:data->front() length:data->size()]
                  scale:scale];
      FaviconAttributes* attributes =
          [FaviconAttributes attributesWithImage:favicon];
      [favicon_cache_ setObject:attributes forKey:key];
      faviconBlockHandler(attributes);
      return;
    }
    // Did not get valid favicon back and are not attempting to retrieve one
    // from a Google Server
    DCHECK(result.fallback_icon_style);
    FaviconAttributes* attributes = [FaviconAttributes
        attributesWithMonogram:base::SysUTF16ToNSString(
                                   favicon::GetFallbackIconText(block_icon_url))
                     textColor:UIColorFromRGB(kFallbackIconDefaultTextColor)
               backgroundColor:UIColor.clearColor
        defaultBackgroundColor:result.fallback_icon_style->
                               is_default_background_color];

    [favicon_cache_ setObject:attributes forKey:key];
    faviconBlockHandler(attributes);
  };

  // First, return a fallback synchronously.
  faviconBlockHandler([FaviconAttributes
      attributesWithImage:[UIImage imageNamed:@"default_world_favicon"]]);

  // Now call the service for a better async icon.
  DCHECK(large_icon_service_);
  large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      icon_url, min_favicon_size_in_pixels, favicon_size_in_pixels,
      base::BindRepeating(favicon_block), &cancelable_task_tracker_);
}

void FaviconLoader::CancellAllRequests() {
  cancelable_task_tracker_.TryCancelAll();
}
