// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_VOICE_SEARCH_TEST_VOICE_SEARCH_H_
#define IOS_CHROME_TEST_PROVIDERS_VOICE_SEARCH_TEST_VOICE_SEARCH_H_

#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

namespace ios {
namespace provider {
namespace test {

// Forces whether Voice Search is enabled or not.
void SetVoiceSearchEnabled(bool enabled);

}  // namespace test
}  // namespace provider
}  // namespace ios

#endif  // IOS_CHROME_TEST_PROVIDERS_VOICE_SEARCH_TEST_VOICE_SEARCH_H_
