// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_web_content_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class PKPass;

@implementation FakeWebContentHandler {
  NSMutableArray* _passes;
}

- (NSArray*)passes {
  return [_passes copy];
}

#pragma mark WebContentCommands

- (void)showAppStoreWithParameters:(NSDictionary*)productParameters {
  self.productParams = productParameters;
}

- (void)showDialogForPassKitPass:(PKPass*)pass {
  if (!_passes) {
    _passes = [[NSMutableArray alloc] init];
  }
  if (pass) {
    [_passes addObject:pass];
  } else {
    [_passes addObject:[NSNull null]];
  }
}

@end
