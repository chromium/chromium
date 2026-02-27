// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"

#import "base/check.h"

@implementation AssistantContainerDetent {
  NSInteger (^_valueResolver)();
}

- (instancetype)initWithIdentifier:(NSString*)identifier
                     valueResolver:(NSInteger (^)())valueResolver {
  self = [super init];
  if (self) {
    CHECK(valueResolver);
    _identifier = [identifier copy];
    _valueResolver = valueResolver;
  }
  return self;
}

- (NSInteger)value {
  return _valueResolver();
}

@end
