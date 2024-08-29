// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/test_account_info_builder.h"

#import "ios/chrome/browser/signin/model/test_account_info.h"

@implementation TestAccountInfoBuilder {
  NSString* _gaiaID;
  NSMutableDictionary<NSString*, NSNumber*>* _capabilities;
}

- (instancetype)initWithTestAccountInfo:(TestAccountInfo*)testAccountInfo {
  if ((self = [super init])) {
    _gaiaID = [testAccountInfo.gaiaID copy];
    self.userEmail = [testAccountInfo.userEmail copy];
    self.userFullName = [testAccountInfo.userFullName copy];
    self.userGivenName = [testAccountInfo.userGivenName copy];
    _capabilities = [testAccountInfo.capabilities mutableCopy];
  }
  return self;
}

- (void)setCapabilityValue:(signin::Tribool)value forName:(NSString*)name {
  switch (value) {
    case signin::Tribool::kUnknown:
      [_capabilities removeObjectForKey:name];
      break;
    case signin::Tribool::kTrue:
      [_capabilities setObject:@YES forKey:name];
      break;
    case signin::Tribool::kFalse:
      [_capabilities setObject:@NO forKey:name];
      break;
  }
}

- (void)resetCapabilityToUnkownValues {
  _capabilities = [NSMutableDictionary dictionary];
}

- (TestAccountInfo*)build {
  return [[TestAccountInfo alloc] initWithUserEmail:self.userEmail
                                             gaiaID:_gaiaID
                                       userFullName:self.userFullName
                                      userGivenName:self.userGivenName
                                       capabilities:_capabilities];
}

@end
