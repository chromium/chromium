// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

class PromosManager;

// A scene agent that checks whether the credential provider extension has been
// enabled by the user and deregisters the promo if needed.
@interface CredentialProviderPromoSceneAgent : ObservingSceneAgent

// Initializes an CredentialProviderPromoSceneAgent instance with given
// PromosManager.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager;

// Unavailable. Use initWithPromosManager:.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_SCENE_AGENT_H_
