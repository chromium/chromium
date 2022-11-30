// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/pasteboard_util.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void StoreURLInPasteboard(const GURL& url) {
  std::vector<const GURL> urls;
  urls.push_back(url);
  StoreURLsInPasteboard(urls);
}

void StoreURLsInPasteboard(const std::vector<const GURL>& urls) {
  DCHECK(!urls.empty());

  NSMutableArray* pasteboard_items = [[NSMutableArray alloc] init];
  for (const GURL& URL : urls) {
    DCHECK(URL.is_valid());
    // Although this breaks the API contract, invalid URLs arrive here in
    // production. Prevent crashing by continuing and early returning below if
    // no valid URLs were passed in `urls`. (crbug.com/880525)
    if (!URL.is_valid()) {
      continue;
    }

    NSData* plainText = [base::SysUTF8ToNSString(URL.spec())
        dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* copiedItem = @{
      (NSString*)kUTTypeURL : net::NSURLWithGURL(URL),
      (NSString*)kUTTypeUTF8PlainText : plainText,
    };

    [pasteboard_items addObject:copiedItem];
  }

  if (!pasteboard_items.count) {
    return;
  }

  [[UIPasteboard generalPasteboard] setItems:pasteboard_items];
}

void StoreInPasteboard(NSString* text, const GURL& URL) {
  DCHECK(text);
  DCHECK(URL.is_valid());
  if (!text || !URL.is_valid()) {
    return;
  }

  NSData* plainText = [base::SysUTF8ToNSString(URL.spec())
      dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* copiedURL = @{
    (NSString*)kUTTypeURL : net::NSURLWithGURL(URL),
    (NSString*)kUTTypeUTF8PlainText : plainText,
  };

  NSDictionary* copiedText = @{
    (NSString*)kUTTypeText : text,
    (NSString*)
    kUTTypeUTF8PlainText : [text dataUsingEncoding:NSUTF8StringEncoding],
  };

  UIPasteboard.generalPasteboard.items = @[ copiedURL, copiedText ];
}

void ClearPasteboard() {
  UIPasteboard.generalPasteboard.items = @[];
}
