// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"

@implementation PasswordImportItem {
  /// Indicates whether favicon loading is in progress.
  BOOL _loadingFavicon;
}
- (instancetype)initWithURL:(NSString*)url
                   username:(NSString*)username
                   password:(NSString*)password {
  self = [super init];
  if (self) {
    _url = url;
    _username = username;
    _password = password;
  }
  return self;
}

- (void)loadFaviconWithCompletionHandler:(UIAction*)handler {
  if (_loadingFavicon) {
    return;
  }
  _loadingFavicon = YES;
  /// TODO(crbug.com/420703283): Implement favicon attribute loading.
}

@end
