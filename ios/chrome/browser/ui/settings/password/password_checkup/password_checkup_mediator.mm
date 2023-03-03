// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordCheckupMediator () <PasswordCheckObserver> {
  // The service responsible for password check feature.
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // A helper object for passing data about changes in password check status
  // and changes to compromised credentials list.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
}

@end

@implementation PasswordCheckupMediator

- (instancetype)initWithPasswordCheckManager:
    (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager {
  self = [super init];
  if (self) {
    _passwordCheckManager = passwordCheckManager;
    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());
  }
  return self;
}

- (void)setConsumer:(id<PasswordCheckupConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
}

- (void)disconnect {
  _passwordCheckObserver.reset();
  _passwordCheckManager.reset();
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  // TODO(crbug.com/1406540): Add method's body.
}

- (void)insecureCredentialsDidChange {
  // TODO(crbug.com/1406540): Add method's body.
}

@end
