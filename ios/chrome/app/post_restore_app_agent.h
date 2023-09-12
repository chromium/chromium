// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_
#define IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"

namespace signin {
class IdentityManager;
}  // namespace signin

class AuthenticationService;
class PrefService;
class PromosManager;

// App agent that displays the Post Restore UI when needed.
@interface PostRestoreAppAgent : NSObject <AppStateAgent>

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                authenticationService:
                    (AuthenticationService*)authenticationService
                      identityManager:(signin::IdentityManager*)identityManager
                           localState:(PrefService*)localState;

@end

// Extension to provide access for unit tests.
@interface PostRestoreAppAgent (Testing) <AppStateObserver>
@end

#endif  // IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_
