// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_web_content_handler.h"

@class PKPass;

@implementation FakeWebContentHandler {
  NSMutableArray* _passes;
}

- (NSArray*)passes {
  return [_passes copy];
}

#pragma mark WebContentCommands

- (void)showAppStoreWithParameters:(NSDictionary*)productParameters {
  self.called = YES;
  self.productParams = productParameters;
}

- (void)showDialogForPassKitPasses:(NSArray<PKPass*>*)passes {
  self.called = YES;
  if (!_passes) {
    _passes = [[NSMutableArray alloc] init];
  }
  if (passes.count > 0) {
    [_passes addObjectsFromArray:passes];
  } else {
    [_passes addObject:[NSNull null]];
  }
}

@end
