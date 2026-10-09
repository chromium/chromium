// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_TEST_TTC_API_H_
#define IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_TEST_TTC_API_H_

#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"

namespace ios::provider::test {

// Overrides the TTC configuration returned by `GetTTCConfig()`.
void SetTTCConfigForTesting(TTCConfig config);

// Resets the TTC configuration back to its default mock value.
void ResetTTCConfigForTesting();

// Overrides the audio engine returned by `CreateTTCAudioEngine()`. Pass nil to
// restore the default behavior.
void SetTTCAudioEngineForTesting(id<TTCAudioEngineProtocol> engine);

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_INTELLIGENCE_TEST_TTC_API_H_
