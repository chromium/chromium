// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_html_element.h"

@implementation CWVHTMLElement
@synthesize hyperlink = _hyperlink;
@synthesize mediaSource = _mediaSource;
@synthesize text = _text;

- (instancetype)initWithHyperlink:(NSURL*)hyperlink
                      mediaSource:(NSURL*)mediaSource
                             text:(NSString*)text {
  self = [super init];
  if (self) {
    _hyperlink = hyperlink;
    _mediaSource = mediaSource;
    _text = [text copy];
  }
  return self;
}

@end
