// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CREDENTIAL_PROVIDER_MIGRATOR_APP_AGENT_H_
#define IOS_CHROME_APP_CREDENTIAL_PROVIDER_MIGRATOR_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// The agent that kicks off the migration of passwords created in the credential
// provider to the password manager.
@interface CredentialProviderAppAgent : SceneObservingAppAgent
@end

#endif  // IOS_CHROME_APP_CREDENTIAL_PROVIDER_MIGRATOR_APP_AGENT_H_
