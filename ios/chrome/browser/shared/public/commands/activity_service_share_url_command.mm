// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/activity_service_share_url_command.h"

#import "url/gurl.h"

@implementation ActivityServiceShareURLCommand

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect {
  if ((self = [super init])) {
    _URL = URL;
    _title = [title copy];
    _sourceView = sourceView;
    _sourceRect = sourceRect;
  }
  return self;
}

@end
