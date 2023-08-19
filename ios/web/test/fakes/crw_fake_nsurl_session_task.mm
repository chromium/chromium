// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_nsurl_session_task.h"

@interface CRWFakeNSURLSessionTask ()
// NSURLSessionTask properties.
@property(nullable, readonly, copy) NSURLRequest* originalRequest;
@property(nullable, readonly, copy) NSURLRequest* currentRequest;
@end

@implementation CRWFakeNSURLSessionTask

@synthesize countOfBytesReceived = _countOfBytesReceived;
@synthesize countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
@synthesize state = _state;
@synthesize originalRequest = _originalRequest;
@synthesize currentRequest = _currentRequest;
@synthesize response = _response;

- (instancetype)initWithURL:(NSURL*)URL {
  _state = NSURLSessionTaskStateSuspended;
  _currentRequest = [NSURLRequest requestWithURL:URL];
  _originalRequest = [NSURLRequest requestWithURL:URL];
  return self;
}

- (void)cancel {
  self.state = NSURLSessionTaskStateCanceling;
}
- (void)resume {
  self.state = NSURLSessionTaskStateRunning;
}

// Below are private methods, called by
// -[NSHTTPCookieStorage storeCookies:forTask:]. Require stubbing in order to
// use NSHTTPCookieStorage API.
- (NSString*)_storagePartitionIdentifier {
  return nil;
}
- (NSURL*)_siteForCookies {
  return nil;
}

@end
