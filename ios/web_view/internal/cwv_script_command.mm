// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_script_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVScriptCommand

@synthesize content = _content;
@synthesize mainDocumentURL = _mainDocumentURL;
@synthesize userInteracting = _userInteracting;

- (instancetype)initWithContent:(nullable NSDictionary*)content
                mainDocumentURL:(NSURL*)mainDocumentURL
                userInteracting:(BOOL)userInteracting {
  self = [super init];
  if (self) {
    _content = [content copy];
    _mainDocumentURL = [mainDocumentURL copy];
    _userInteracting = userInteracting;
  }
  return self;
}

@end
