// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_user_script.h"

@implementation CWVUserScript

@synthesize source = _source;
@synthesize forMainFrameOnly = _forMainFrameOnly;
@synthesize injectionTime = _injectionTime;

- (nonnull instancetype)initWithSource:(nonnull NSString*)source {
  return [self initWithSource:source forMainFrameOnly:true];
}

- (nonnull instancetype)initWithSource:(nonnull NSString*)source
                      forMainFrameOnly:(BOOL)forMainFrameOnly {
  return [self initWithSource:source
             forMainFrameOnly:forMainFrameOnly
                injectionTime:CWVUserScriptInjectionTimeAtDocumentStart];
}

- (instancetype)initWithSource:(NSString*)source
              forMainFrameOnly:(BOOL)forMainFrameOnly
                 injectionTime:(CWVUserScriptInjectionTime)injectionTime {
  self = [super init];
  if (self) {
    _source = [source copy];
    _forMainFrameOnly = forMainFrameOnly;
    _injectionTime = injectionTime;
  }
  return self;
}

@end
