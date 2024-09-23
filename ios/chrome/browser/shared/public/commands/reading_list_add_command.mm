// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"

#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "url/gurl.h"

@interface ReadingListAddCommand ()

@property(nonatomic, strong) NSArray<URLWithTitle*>* URLs;

@end

@implementation ReadingListAddCommand

@synthesize URLs = _URLs;

- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title {
  if ((self = [super init])) {
    _URLs = @[ [[URLWithTitle alloc] initWithURL:URL title:title] ];
  }
  return self;
}

- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs {
  if ((self = [super init])) {
    _URLs = [URLs copy];
  }
  return self;
}

@end
