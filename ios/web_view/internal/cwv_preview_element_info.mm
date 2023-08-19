// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_preview_element_info.h"
#import "ios/web_view/internal/cwv_preview_element_info_internal.h"

@implementation CWVPreviewElementInfo

@synthesize linkURL = _linkURL;

- (instancetype)initWithLinkURL:(NSURL*)linkURL {
  self = [super init];
  if (self) {
    _linkURL = linkURL;
  }
  return self;
}

@end
