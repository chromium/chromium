// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CERTIFICATE_POLICY_APP_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CERTIFICATE_POLICY_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// An app agent that handles updating the certificate policy caches when the
// app backgrounds -- evicting cached entries that no open tabs are using.
@interface CertificatePolicyAppAgent : SceneObservingAppAgent

// YES if cache updates are in progress (on the IO thread).
@property(nonatomic, readonly, getter=isWorking) BOOL working;

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CERTIFICATE_POLICY_APP_AGENT_H_
