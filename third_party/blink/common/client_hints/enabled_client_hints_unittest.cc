// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

using ::network::mojom::WebClientHintsType;
using ::testing::ElementsAre;

class EnabledClientHintsTest : public testing::Test {
 public:
  EnabledClientHintsTest()
      : response_headers_(base::MakeRefCounted<net::HttpResponseHeaders>("")) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            blink::features::kClientHintsDeviceMemory_DEPRECATED});
  }

  const net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

TEST_F(EnabledClientHintsTest, EnabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, true);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, true);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, true);
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kUAFullVersionList));
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kRtt_DEPRECATED));
}

TEST_F(EnabledClientHintsTest, DisabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, false);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, false);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, false);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kUAFullVersionList));
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kRtt_DEPRECATED));
}

TEST_F(EnabledClientHintsTest, EnabledClientHintOnDisabledFeature) {
  EnabledClientHints hints;
  // Attempting to enable the device-memory-deprecated client hint, but the
  // feature for it is disabled.
  hints.SetIsEnabled(WebClientHintsType::kDeviceMemory_DEPRECATED, true);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kDeviceMemory_DEPRECATED));
}

TEST_F(EnabledClientHintsTest, GetEnabledHints) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, true);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, true);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, true);
  EXPECT_THAT(hints.GetEnabledHints(),
              ElementsAre(WebClientHintsType::kRtt_DEPRECATED,
                          WebClientHintsType::kUAFullVersion,
                          WebClientHintsType::kUAFullVersionList));
}

}  // namespace blink
