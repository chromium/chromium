// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_EMBEDDED_REAUTH_H_
#define IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_EMBEDDED_REAUTH_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/public/provider/chrome/browser/signin/embedded_reauth_interaction_manager.h"

namespace ios::provider::test {

// Sets whether `TestEmbeddedReAuthInteractionManager::IsReAuthSupported()`
// returns `supported` for testing.
void SetEmbeddedReAuthSupported(bool supported);

// Sets the result (`reauth_proof_token`, `error`) returned by
// `TestEmbeddedReAuthInteractionManager` for testing.
void SetEmbeddedReAuthResult(NSString* reauth_proof_token, NSError* error);

// Returns the last `reauth_token` passed to `InitiateEmbeddedReAuth`, or `nil`
// if `InitiateEmbeddedReAuth` was not called.
NSString* GetLastEmbeddedReAuthToken();

// Returns the last `token_type` passed to `InitiateEmbeddedReAuth`, or
// `std::nullopt` if `InitiateEmbeddedReAuth` was not called.
std::optional<EmbeddedReAuthTokenType> GetLastEmbeddedReAuthTokenType();

// Resets the `TestEmbeddedReAuthInteractionManager` test configuration and
// recorded arguments to their default values.
void ResetEmbeddedReAuthState();

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_EMBEDDED_REAUTH_H_
