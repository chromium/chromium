// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/fake_observable_boolean.h"

@implementation FakeObservableBoolean

@synthesize value = _value;
@synthesize observer = _observer;

- (void)setValue:(BOOL)value {
  if (_value != value) {
    _value = value;
    [_observer booleanDidChange:self];
  }
}

@end

@implementation TestBooleanObserver

@synthesize updateCount = _updateCount;

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  _updateCount++;
}

@end
