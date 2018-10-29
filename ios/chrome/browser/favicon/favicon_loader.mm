// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_loader.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/core/fallback_url_util.h"
#include "components/favicon/core/favicon_server_fetcher_params.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_callback.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"
#include "skia/ext/skia_utils_ios.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
extern const CGFloat kFallbackIconDefaultTextColor = 0xAAAAAA;

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
FaviconAttributes* FaviconLoader::FaviconForUrl(
    const GURL& url,
    float size,
    float min_size,
    bool fallback_to_google_server,  // retrieve favicon from Google Server if
                                     // GetLargeIconOrFallbackStyle() doesn't
                                     // return valid favicon.
    FaviconAttributesCompletionBlock block) {
  NSString* key = base::SysUTF8ToNSString(url.spec());
  FaviconAttributes* value = [favicon_cache_ objectForKey:key];
  if (value) {
    return value;
  }

  GURL block_url(url);
  auto favicon_block = ^(const favicon_base::LargeIconResult& result) {
    // GetLargeIconOrFallbackStyle() either returns a valid favicon (which can
    // be the default favicon) or fallback attributes.
    if (result.bitmap.is_valid()) {
      scoped_refptr<base::RefCountedMemory> data =
          result.bitmap.bitmap_data.get();
      // The favicon code assumes favicons are PNG-encoded.
      UIImage* favicon =
          [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                length:data->size()]];
      FaviconAttributes* attributes =
          [FaviconAttributes attributesWithImage:favicon];
      [favicon_cache_ setObject:attributes forKey:key];
      block(attributes);
      return;
    } else if (fallback_to_google_server) {
      void (^favicon_loaded_from_server_block)(
          favicon_base::GoogleFaviconServerRequestStatus status) =
          ^(const favicon_base::GoogleFaviconServerRequestStatus status) {
            // Update the time when the icon was last requested - postpone thus
            // the automatic eviction of the favicon from the favicon database.
            large_icon_service_->TouchIconFromGoogleServer(block_url);

            // Favicon should be loaded to the db that backs LargeIconService
            // now.  Fetch it again. Even if the request was not successful, the
            // fallback style will be used.
            FaviconForUrl(block_url, size, min_size,
                          /*continueToGoogleServer=*/false, block);

          };

      large_icon_service_
          ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
              favicon::FaviconServerFetcherParams::CreateForMobile(
                  block_url, min_size, size),
              /*may_page_url_be_private=*/true, kTrafficAnnotation,
              base::BindRepeating(favicon_loaded_from_server_block));
      return;
    }

    // Did not get valid favicon back and are not attempting to retrieve one
    // from a Google Server
    DCHECK(result.fallback_icon_style);
    UIColor* textColor =
        skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
    UIColor* backgroundColor =
        skia::UIColorFromSkColor(result.fallback_icon_style->background_color);
    if (IsUIRefreshPhase1Enabled()) {
      textColor = UIColorFromRGB(kFallbackIconDefaultTextColor);
      backgroundColor = [UIColor clearColor];
    }
    FaviconAttributes* attributes = [FaviconAttributes
        attributesWithMonogram:base::SysUTF16ToNSString(
                                   favicon::GetFallbackIconText(block_url))
                     textColor:textColor
               backgroundColor:backgroundColor
        defaultBackgroundColor:result.fallback_icon_style->
                               is_default_background_color];

    [favicon_cache_ setObject:attributes forKey:key];
    block(attributes);
  };

  CGFloat favicon_size_in_pixels = [UIScreen mainScreen].scale * size;
  CGFloat min_favicon_size = [UIScreen mainScreen].scale * min_size;
  DCHECK(large_icon_service_);
  large_icon_service_->GetLargeIconOrFallbackStyle(
      block_url, min_favicon_size, favicon_size_in_pixels,
      base::BindRepeating(favicon_block), &cancelable_task_tracker_);

  if (IsUIRefreshPhase1Enabled()) {
    return [FaviconAttributes
        attributesWithImage:[UIImage imageNamed:@"default_world_favicon"]];
  }
  return [FaviconAttributes
      attributesWithImage:[UIImage imageNamed:@"default_favicon"]];
}

void FaviconLoader::CancellAllRequests() {
  cancelable_task_tracker_.TryCancelAll();
}
