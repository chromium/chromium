// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"

#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

// Helper class implementing SuccessfulReauthTimeAccessor protocol.
@interface SuccessfulReauthTimeAccessorImpl
    : NSObject <SuccessfulReauthTimeAccessor>

@property(nonatomic, strong) NSDate* lastSuccessfulReauthTime;

@end

@implementation SuccessfulReauthTimeAccessorImpl

- (void)updateSuccessfulReauthTime {
  self.lastSuccessfulReauthTime = [[NSDate alloc] init];
}

@end

ReauthenticationService::ReauthenticationService(
    id<ReauthenticationProtocol> reauth_module) {
  reauth_time_accessor_ = [[SuccessfulReauthTimeAccessorImpl alloc] init];
  if (reauth_module) {
    reauth_module_ = reauth_module;
  } else {
    reauth_module_ = [[ReauthenticationModule alloc]
        initWithSuccessfulReauthTimeAccessor:reauth_time_accessor_];
  }
}

ReauthenticationService::~ReauthenticationService() = default;

id<ReauthenticationProtocol> ReauthenticationService::GetReauthModule() {
  return reauth_module_;
}
