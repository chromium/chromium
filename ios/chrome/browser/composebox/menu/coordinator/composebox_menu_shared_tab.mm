// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"

#import "base/unguessable_token.h"

@implementation ComposeboxMenuSharedTab

- (instancetype)initWithURL:(GURL)URL
                      title:(NSString*)title
                serverToken:(base::UnguessableToken)serverToken
                    favicon:(UIImage*)favicon {
  self = [super init];
  if (self) {
    _URL = URL;
    _title = [title copy];
    _serverToken = serverToken;
    _favicon = favicon;
  }
  return self;
}

@end
