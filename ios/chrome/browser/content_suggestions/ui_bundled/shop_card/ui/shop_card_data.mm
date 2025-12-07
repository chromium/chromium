// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_data.h"

#import "url/gurl.h"

@implementation ShopCardData {
  GURL _productURL;
}

#pragma mark - properties

- (const GURL&)productURL {
  return _productURL;
}

@end
