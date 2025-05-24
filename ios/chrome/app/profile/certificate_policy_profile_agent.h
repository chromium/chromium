// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_CERTIFICATE_POLICY_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_CERTIFICATE_POLICY_PROFILE_AGENT_H_

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

// Profile agent that handles updating the certificate policy caches when the
// app backgrounds -- evicting cached entries that no open tabs are using.
@interface CertificatePolicyProfileAgent : SceneObservingProfileAgent

// YES if cache updates are in progress (on the IO thread).
@property(nonatomic, readonly, getter=isWorking) BOOL working;

@end

#endif  // IOS_CHROME_APP_PROFILE_CERTIFICATE_POLICY_PROFILE_AGENT_H_
