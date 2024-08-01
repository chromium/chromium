// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_site_info.h"

#import "url/gurl.h"

@interface ManualFillSiteInfo () {
  // iVar to backup URL.
  GURL _URL;
}
@end

@implementation ManualFillSiteInfo

- (instancetype)initWithSiteName:(NSString*)siteName
                            host:(NSString*)host
                             URL:(const GURL&)URL {
  self = [super init];
  if (self) {
    _host = [host copy];
    _siteName = [siteName copy];
    _URL = URL;
  }
  return self;
}

- (const GURL&)URL {
  return _URL;
}

@end
