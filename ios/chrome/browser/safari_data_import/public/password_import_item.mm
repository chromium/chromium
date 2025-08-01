// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"

#import "ios/chrome/browser/safari_data_import/public/password_import_item_favicon_data_source.h"

@implementation PasswordImportItem {
  /// Indicates whether favicon loading is initiated.
  BOOL _faviconLoadingInitiated;
}
- (instancetype)initWithURL:(NSString*)url
                   username:(NSString*)username
                   password:(NSString*)password
                     status:(PasswordImportStatus)status {
  self = [super init];
  if (self) {
    _url = url;
    _username = username;
    _password = password;
    _status = status;
  }
  return self;
}

- (void)loadFaviconWithCompletionHandler:(ProceduralBlock)handler {
  if (_faviconLoadingInitiated) {
    return;
  }
  _faviconLoadingInitiated =
      [self.faviconDataSource passwordImportItem:self
             loadFaviconAttributesWithCompletion:handler];
}

@end
