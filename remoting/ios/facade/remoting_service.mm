// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/facade/remoting_service.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "base/memory/raw_ptr.h"
#import "remoting/ios/domain/user_info.h"
#import "remoting/ios/facade/ios_client_runtime_delegate.h"
#import "remoting/ios/facade/remoting_authentication.h"

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

NSString* const kUserDidUpdate = @"kUserDidUpdate";
NSString* const kUserInfo = @"kUserInfo";

@interface RemotingService ()<RemotingAuthenticationDelegate> {
  id<RemotingAuthentication> _authentication;

  // TODO(yuweih): It's suspicious to use a raw C++ pointer here. Investigate
  // its lifetime in ChromotingRuntime and change to unique_ptr if possible.
  raw_ptr<remoting::IosClientRuntimeDelegate> _clientRuntimeDelegate;
}
@end

@implementation RemotingService

// RemotingService is a singleton.
+ (RemotingService*)instance {
  static RemotingService* sharedInstance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    sharedInstance = [[RemotingService alloc] init];
  });
  return sharedInstance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _clientRuntimeDelegate =
        new remoting::IosClientRuntimeDelegate();
    [self runtime]->Init(_clientRuntimeDelegate);
  }
  return self;
}

#pragma mark - RemotingAuthenticationDelegate

- (void)userDidUpdate:(UserInfo*)user {
  NSDictionary* userInfo =
      user ? [NSDictionary dictionaryWithObject:user forKey:kUserInfo] : nil;
  [[NSNotificationCenter defaultCenter] postNotificationName:kUserDidUpdate
                                                      object:self
                                                    userInfo:userInfo];
}

#pragma mark - Properties

- (remoting::ChromotingClientRuntime*)runtime {
  return remoting::ChromotingClientRuntime::GetInstance();
}

#pragma mark - Implementation

- (void)setAuthentication:(id<RemotingAuthentication>)authentication {
  DCHECK(_authentication == nil);
  authentication.delegate = self;
  _authentication = authentication;
}

- (id<RemotingAuthentication>)authentication {
  DCHECK(_authentication != nil);
  return _authentication;
}

@end
