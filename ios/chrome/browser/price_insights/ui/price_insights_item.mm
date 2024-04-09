// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

#import "url/gurl.h"

@implementation PriceInsightsItem {
  GURL _buyingOptionsURL;
  GURL _productURL;
}

#pragma mark - Properties

- (const GURL&)buyingOptionsURL {
  return _buyingOptionsURL;
}

- (void)setBuyingOptionsUrl:(const GURL&)url {
  _buyingOptionsURL = url;
}

- (const GURL&)productURL {
  return _productURL;
}

- (void)setProductUrl:(const GURL&)url {
  _productURL = url;
}

@end
