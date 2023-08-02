// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_manager.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>
#import "base/check.h"
#import "ios/chrome/browser/ui/settings/utils/password_auto_fill_status_observer.h"

@interface PasswordAutoFillStatusManager ()

// Weak references to the auto-fill status manager's observer objects.
@property(nonatomic, strong)
    NSHashTable<id<PasswordAutoFillStatusObserver>>* observers;

// Overwrite readonly properties in the header to be writable internally.
@property(nonatomic, assign, readwrite) BOOL ready;
@property(nonatomic, assign, readwrite) BOOL autoFillEnabled;

@end

@implementation PasswordAutoFillStatusManager

+ (PasswordAutoFillStatusManager*)sharedManager {
  static dispatch_once_t onceToken;
  static PasswordAutoFillStatusManager* sharedManager = nil;
  dispatch_once(&onceToken, ^{
    sharedManager = [[PasswordAutoFillStatusManager alloc] init];
  });
  return sharedManager;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)addObserver:(id<PasswordAutoFillStatusObserver>)observer {
  [self.observers addObject:observer];
  [self checkAndUpdatePasswordAutoFillStatus];
  if (self.observers.count == 1) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
}

- (void)removeObserver:(id<PasswordAutoFillStatusObserver>)observer {
  [self.observers removeObject:observer];
  if (self.observers.count == 0) {
    self.ready = NO;
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:UIApplicationWillEnterForegroundNotification
                object:nil];
  }
}

- (void)applicationWillEnterForeground:(NSNotification*)notification {
  [self checkAndUpdatePasswordAutoFillStatus];
}

- (void)checkAndUpdatePasswordAutoFillStatus {
  [ASCredentialIdentityStore
          .sharedStore getCredentialIdentityStoreStateWithCompletion:^(
                           ASCredentialIdentityStoreState* state) {
    dispatch_async(dispatch_get_main_queue(), ^{
      // The completion handler sent to ASCredentialIdentityStore is executed on
      // a background thread. Putting it back onto the main thread to handle
      // prospective UI changes.
      BOOL isEnabled = state.isEnabled;
      if (!self.ready || self.autoFillEnabled != isEnabled) {
        self.autoFillEnabled = isEnabled;
        self.ready = YES;
        for (id<PasswordAutoFillStatusObserver> observer in self.observers) {
          [observer passwordAutoFillStatusDidChange];
        }
      }
    });
  }];
}

@end
