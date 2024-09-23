// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_IDENTITY_CONFIRMATION_APP_AGENT_H_
#define IOS_CHROME_APP_IDENTITY_CONFIRMATION_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// App agent that triggers the signed-in identity confirmation when necessary.
@interface IdentityConfirmationAppAgent : SceneObservingAppAgent

@end

#endif  // IOS_CHROME_APP_IDENTITY_CONFIRMATION_APP_AGENT_H_
