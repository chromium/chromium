// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_password_auto_fill_status_manager.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"

// This interface declaration provides access to the super class's private
// property self.observers.
@interface PasswordAutoFillStatusManager ()
@property(nonatomic, strong)
    NSHashTable<id<PasswordAutoFillStatusObserver>>* observers;

// Overwrite readonly properties in the header to be writable internally.
@property(nonatomic, assign, readwrite) BOOL ready;
@property(nonatomic, assign, readwrite) BOOL autoFillEnabled;
@end

@implementation FakePasswordAutoFillStatusManager

+ (FakePasswordAutoFillStatusManager*)sharedFakeManager {
  static dispatch_once_t onceToken;
  static FakePasswordAutoFillStatusManager* sharedManager = nil;
  dispatch_once(&onceToken, ^{
    sharedManager = [[FakePasswordAutoFillStatusManager alloc] init];
  });
  return sharedManager;
}

- (void)addObserver:(id<PasswordAutoFillStatusObserver>)observer {
  [self.observers addObject:observer];
}

- (void)removeObserver:(id<PasswordAutoFillStatusObserver>)observer {
  [self.observers removeObject:observer];
}

- (void)startFakeManagerWithAutoFillStatus:(BOOL)autoFillEnabled {
  self.ready = YES;
  self.autoFillEnabled = autoFillEnabled;
  for (id<PasswordAutoFillStatusObserver> observer in self.observers) {
    [observer passwordAutoFillStatusDidChange];
  }
}

- (void)toggleAutoFillStatus {
  self.autoFillEnabled = !self.autoFillEnabled;
  for (id<PasswordAutoFillStatusObserver> observer in self.observers) {
    [observer passwordAutoFillStatusDidChange];
  }
}

- (void)setAutoFillStatus:(BOOL)autoFillEnabled {
  if (self.autoFillEnabled != autoFillEnabled) {
    self.autoFillEnabled = autoFillEnabled;
    for (id<PasswordAutoFillStatusObserver> observer in self.observers) {
      [observer passwordAutoFillStatusDidChange];
    }
  }
}

- (void)reset {
  [self.observers removeAllObjects];
  self.ready = NO;
}

@end
