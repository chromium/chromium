// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_util.h"

#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSSet<NSString*>* GetItmsSchemes() {
  static NSSet<NSString*>* schemes;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    schemes = [NSSet<NSString*>
        setWithObjects:@"itms", @"itmss", @"itms-apps", @"itms-appss",
                       // There's no evidence that itms-bookss is actually
                       // supported, but over-inclusion costs less than
                       // under-inclusion.
                       @"itms-books", @"itms-bookss", nil];
  });
  return schemes;
}

bool UrlHasAppStoreScheme(const GURL& url) {
  return
      [GetItmsSchemes() containsObject:base::SysUTF8ToNSString(url.scheme())];
}
