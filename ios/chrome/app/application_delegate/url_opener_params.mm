// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener_params.h"

@implementation URLOpenerParams

- (instancetype)initWithURL:(NSURL*)URL
          sourceApplication:(NSString*)sourceApplication {
  self = [super init];
  if (self) {
    _URL = URL;
    _sourceApplication = sourceApplication;
  }
  return self;
}

- (instancetype)initWithUIOpenURLContext:(UIOpenURLContext*)context {
  return [self initWithURL:context.URL
         sourceApplication:context.options.sourceApplication];
}

@end
