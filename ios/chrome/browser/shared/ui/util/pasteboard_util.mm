// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/functional/bind.h"
#import "base/functional/callback_forward.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/open_from_clipboard/clipboard_async_wrapper_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

void StoreURLInPasteboard(const GURL& url) {
  StoreURLInPasteboard(url, base::DoNothing());
}

void StoreURLInPasteboard(const GURL& url, base::OnceClosure completion) {
  StoreURLsInPasteboard({url}, std::move(completion));
}

void StoreURLsInPasteboard(const std::vector<GURL>& urls) {
  StoreURLsInPasteboard(urls, base::DoNothing());
}

void StoreURLsInPasteboard(const std::vector<GURL>& urls,
                           base::OnceClosure completion) {
  NSMutableArray* pasteboard_items = [[NSMutableArray alloc] init];
  for (const GURL& URL : urls) {
    // Invalid URLs arrive here in production. Prevent crashing by continuing
    // and early returning below if no valid URLs were passed in `urls`.
    // (crbug.com/880525)
    if (!URL.is_valid()) {
      continue;
    }

    NSMutableDictionary* copiedItem = [[NSMutableDictionary alloc] init];

    NSURL* nsURL = net::NSURLWithGURL(URL);
    if (nsURL) {
      copiedItem[UTTypeURL.identifier] = nsURL;
    }

    NSData* plainText = [base::SysUTF8ToNSString(URL.spec())
        dataUsingEncoding:NSUTF8StringEncoding];
    if (plainText) {
      copiedItem[UTTypeUTF8PlainText.identifier] = plainText;
    }

    [pasteboard_items addObject:copiedItem];
  }

  if (!pasteboard_items.count) {
    return;
  }

  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         [pasteboard setItems:pasteboard_items];
                       }).Then(std::move(completion)));
}

void StoreInPasteboard(NSString* text, const GURL& url) {
  StoreInPasteboard(text, url, base::DoNothing());
}

void StoreInPasteboard(NSString* text,
                       const GURL& url,
                       base::OnceClosure completion) {
  DCHECK(text);
  DCHECK(url.is_valid());
  if (!text || !url.is_valid()) {
    return;
  }

  NSData* plainText = [base::SysUTF8ToNSString(url.spec())
      dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* copiedURL = @{
    UTTypeURL.identifier : net::NSURLWithGURL(url),
    UTTypeUTF8PlainText.identifier : plainText,
  };

  NSDictionary* copiedText = @{
    UTTypeText.identifier : text,
    UTTypeUTF8PlainText.
    identifier : [text dataUsingEncoding:NSUTF8StringEncoding],
  };

  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         pasteboard.items = @[ copiedURL, copiedText ];
                       }).Then(std::move(completion)));
}

void StoreTextInPasteboard(NSString* text) {
  StoreTextInPasteboard(text, base::DoNothing());
}

void StoreTextInPasteboard(NSString* text, base::OnceClosure completion) {
  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         pasteboard.string = text;
                       }).Then(std::move(completion)));
}

ImageCopyResult StoreImageInPasteboard(NSData* data, NSURL* url) {
  return StoreImageInPasteboard(data, url, base::DoNothing());
}

ImageCopyResult StoreImageInPasteboard(NSData* data,
                                       NSURL* url,
                                       base::OnceClosure completion) {
  // Copy image data to pasteboard. Don't copy the URL otherwise some apps
  // will paste the text and not the image. See crbug.com/1270239.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:1];
  NSString* uti = GetImageUTIFromData(data);
  ImageCopyResult result;
  if (uti) {
    [item setValue:data forKey:uti];
    result = ImageCopyResult::kImage;
  } else {
    [item setValue:url forKey:UTTypeURL.identifier];

    result = ImageCopyResult::kURL;
  }
  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         pasteboard.items =
                             [NSMutableArray arrayWithObject:item];
                       }).Then(std::move(completion)));
  return result;
}

void StoreItemInPasteboard(NSDictionary* item) {
  StoreItemInPasteboard(item, base::DoNothing());
}

void StoreItemInPasteboard(NSDictionary* item, base::OnceClosure completion) {
  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         pasteboard.items = [NSArray arrayWithObject:item];
                       }).Then(std::move(completion)));
}

void ClearPasteboard() {
  ClearPasteboard(base::DoNothing());
}

void ClearPasteboard(base::OnceClosure completion) {
  GetGeneralPasteboard(base::FeatureList::IsEnabled(kOnlyAccessClipboardAsync),
                       base::BindOnce(^(UIPasteboard* pasteboard) {
                         pasteboard.items = @[];
                       }).Then(std::move(completion)));
}
