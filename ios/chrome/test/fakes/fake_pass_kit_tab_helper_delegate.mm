// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_pass_kit_tab_helper_delegate.h"

#import "ios/chrome/browser/download/pass_kit_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakePassKitTabHelperDelegate {
  web::WebState* _webState;
  NSMutableArray* _passes;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
    _passes = [NSMutableArray array];
  }
  return self;
}

- (NSArray*)passes {
  return [_passes copy];
}

- (void)passKitTabHelper:(nonnull PassKitTabHelper*)tabHelper
    presentDialogForPass:(nullable PKPass*)pass
                webState:(nonnull web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  DCHECK_EQ(PassKitTabHelper::FromWebState(_webState), tabHelper);
  if (pass) {
    [_passes addObject:pass];
  } else {
    [_passes addObject:[NSNull null]];
  }
}

@end
