// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

using ::network::mojom::WebClientHintsType;

class EnabledClientHintsTest : public testing::Test {
 public:
  EnabledClientHintsTest() {
    // The UserAgentClientHint feature is enabled, and the LangClientHintHeader
    // feature is disabled.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kUserAgentClientHint},
        /*disabled_features=*/{blink::features::kLangClientHintHeader});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EnabledClientHintsTest, EnabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, true);
  hints.SetIsEnabled(WebClientHintsType::kRtt, true);
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kRtt));
}

TEST_F(EnabledClientHintsTest, DisabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, false);
  hints.SetIsEnabled(WebClientHintsType::kRtt, false);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kRtt));
}

TEST_F(EnabledClientHintsTest, EnabledClientHintOnDisabledFeature) {
  EnabledClientHints hints;
  // Attempting to enable the lang client hint, but the runtime flag for it is
  // disabled.
  hints.SetIsEnabled(WebClientHintsType::kLang, true);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kLang));
}

}  // namespace blink
