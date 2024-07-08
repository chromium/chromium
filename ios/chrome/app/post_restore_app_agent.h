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
class PromosManager;

// App agent that displays the Post Restore UI when needed.
// TODO(crbug.com/325616341): This needs to handle multiple browser states in
// some way. All of the services passed in are specific to a single browser
// state, so the functionality needs to be clarified for multiple identities.
@interface PostRestoreAppAgent : NSObject <AppStateAgent>

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                authenticationService:
                    (AuthenticationService*)authenticationService
                      identityManager:(signin::IdentityManager*)identityManager;

@end

// Extension to provide access for unit tests.
@interface PostRestoreAppAgent (Testing) <AppStateObserver>
@end

#endif  // IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_
