// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/credential_import_item.h"

#import "ios/chrome/browser/data_import/public/credential_import_item_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"

@implementation CredentialImportItem {
  /// Indicates whether favicon loading is initiated.
  BOOL _faviconLoadingInitiated;
}

- (instancetype)initWithUrl:(URLWithTitle*)url username:(NSString*)username {
  self = [super init];
  if (self) {
    _url = url;
    _username = username;
  }
  return self;
}

- (void)loadFaviconWithUIUpdateHandler:(ProceduralBlock)handler {
  if (_faviconLoadingInitiated) {
    return;
  }
  _faviconLoadingInitiated =
      [self.faviconDataSource credentialImportItem:self
                loadFaviconAttributesWithUIHandler:handler];
}

@end
