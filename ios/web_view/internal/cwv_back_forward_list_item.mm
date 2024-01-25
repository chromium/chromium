// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_back_forward_list_item_internal.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "net/base/apple/url_conversions.h"

@implementation CWVBackForwardListItem

- (instancetype)initWithNavigationItem:
    (const web::NavigationItem*)navigationItem {
  // WARNING: |navigationItem| should not be stored since it can be freed by
  // owner (the lower level web::NavigationManager) anytime.

  self = [super init];
  if (self) {
    _uniqueID = navigationItem->GetUniqueID();

    _title = base::SysUTF16ToNSString(navigationItem->GetTitle());
    _URL = net::NSURLWithGURL(navigationItem->GetURL());
  }
  return self;
}

- (BOOL)isEqual:(id)other {
  if (other == self) {
    return YES;
  }

  if (![other isKindOfClass:self.class]) {
    return NO;
  }

  return self.uniqueID ==
         base::apple::ObjCCastStrict<CWVBackForwardListItem>(other).uniqueID;
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.uniqueID);
}

@end
