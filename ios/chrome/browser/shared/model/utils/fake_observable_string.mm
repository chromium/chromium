// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/fake_observable_string.h"

@implementation FakeObservableString

@synthesize value = _value;
@synthesize observer = _observer;

- (instancetype)init {
  if ((self = [super init])) {
    _value = @"";
  }
  return self;
}

- (void)setValue:(NSString*)value {
  if (![_value isEqualToString:value]) {
    _value = value ? [value copy] : @"";
    [_observer stringDidChange:self];
  }
}

@end

@implementation TestStringObserver

@synthesize updateCount = _updateCount;

- (void)stringDidChange:(id<ObservableString>)ObservableString {
  _updateCount++;
}

@end
