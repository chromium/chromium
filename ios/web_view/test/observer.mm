// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/observer.h"

@implementation Observer

@synthesize keyPath = _keyPath;
@synthesize lastValue = _lastValue;
@synthesize object = _object;
@synthesize previousValue = _previousValue;

- (void)setObservedObject:(NSObject*)object keyPath:(NSString*)keyPath {
  [_object removeObserver:self forKeyPath:_keyPath];

  _lastValue = nil;
  _previousValue = nil;
  _keyPath = [keyPath copy];
  _object = object;
  [_object addObserver:self
            forKeyPath:_keyPath
               options:NSKeyValueObservingOptionNew
               context:nil];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if (![object isEqual:_object] || ![keyPath isEqualToString:_keyPath]) {
    // Ignore extraneous call from previous |_object| or |_keyPath|.
    return;
  }
  _previousValue = _lastValue;
  _lastValue = change[NSKeyValueChangeNewKey];
}

- (void)dealloc {
  [_object removeObserver:self forKeyPath:_keyPath];
}

@end
