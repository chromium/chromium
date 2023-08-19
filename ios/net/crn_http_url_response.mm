// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_url_response.h"


@interface CRNHTTPURLResponse () {
  NSString* _cr_HTTPVersion;
}
@end

@implementation CRNHTTPURLResponse

- (NSString*)cr_HTTPVersion {
  return _cr_HTTPVersion;
}

- (instancetype)initWithURL:(NSURL*)url
                 statusCode:(NSInteger)statusCode
                HTTPVersion:(NSString*)HTTPVersion
               headerFields:(NSDictionary*)headerFields {
  self = [super initWithURL:url
                 statusCode:statusCode
                HTTPVersion:HTTPVersion
               headerFields:headerFields];
  if (self) {
    _cr_HTTPVersion = [HTTPVersion copy];
  }
  return self;
}

@end
