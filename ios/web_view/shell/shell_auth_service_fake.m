// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_auth_service.h"

// Fake implementation of ShellAuthService.
@implementation ShellAuthService

- (NSArray<CWVIdentity*>*)identities {
  return @[];
}

#pragma mark CWVSyncControllerDataSource

- (void)fetchAccessTokenForIdentity:(CWVIdentity*)identity
                             scopes:(NSArray<NSString*>*)scopes
                  completionHandler:
                      (void (^)(NSString* _Nullable accessToken,
                                NSDate* _Nullable expirationDate,
                                NSError* _Nullable error))completionHandler {
  // Always returns an error.
  if (completionHandler) {
    completionHandler(
        nil, nil,
        [NSError errorWithDomain:@"org.chromium.chromewebview.shell"
                            code:0
                        userInfo:nil]);
  }
}

- (NSArray<CWVIdentity*>*)allKnownIdentities {
  return [self identities];
}

- (CWVSyncError)syncErrorForNSError:(NSError*)error
                           identity:(CWVIdentity*)identity {
  return CWVSyncErrorUnexpectedServiceResponse;
}

@end
