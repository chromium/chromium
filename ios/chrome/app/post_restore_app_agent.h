// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_
#define IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"

// App agent that displays the Post Restore UI when needed.
@interface PostRestoreAppAgent : NSObject <AppStateAgent>
@end

// Extension to provide access for unit tests to easily inject state.
@interface PostRestoreAppAgent (Testing) <AppStateObserver>

// Stores the PromosManager, which is used to register the post restore
// sign-in promo, when appropriate.
@property(nonatomic) PromosManager* promosManager;

@end

#endif  // IOS_CHROME_APP_POST_RESTORE_APP_AGENT_H_
