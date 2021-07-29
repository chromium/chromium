// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/client_hints.h"

#include <iostream>

#include "base/test/scoped_feature_list.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using testing::UnorderedElementsAre;

namespace blink {

TEST(ClientHintsTest, SerializeLangClientHint) {
  std::string header = SerializeLangClientHint("");
  EXPECT_TRUE(header.empty());

  header = SerializeLangClientHint("es");
  EXPECT_EQ(std::string("\"es\""), header);

  header = SerializeLangClientHint("en-US,fr,de");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\""), header);

  header = SerializeLangClientHint("en-US,fr,de,ko,zh-CN,ja");
  EXPECT_EQ(std::string("\"en-US\", \"fr\", \"de\", \"ko\", \"zh-CN\", \"ja\""),
            header);
}

// Checks that the removed header list doesn't include legacy headers nor the
// on-by-default ones, when the kAllowClientHintsToThirdParty flag is on.
TEST(ClientHintsTest, FindClientHintsToRemoveLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAllowClientHintsToThirdParty);
  std::vector<std::string> removed_headers;
  FindClientHintsToRemove(nullptr, GURL(), &removed_headers);
  EXPECT_THAT(removed_headers,
              UnorderedElementsAre(
                  "rtt", "downlink", "ect", "sec-ch-lang", "sec-ch-ua-arch",
                  "sec-ch-ua-model", "sec-ch-ua-full-version",
                  "sec-ch-ua-platform-version", "sec-ch-prefers-color-scheme",
                  "sec-ch-ua-bitness", "sec-ch-ua-reduced"));
}

// Checks that the removed header list includes legacy headers but not the
// on-by-default ones, when the kAllowClientHintsToThirdParty flag is off.
TEST(ClientHintsTest, FindClientHintsToRemoveNoLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAllowClientHintsToThirdParty);
  std::vector<std::string> removed_headers;
  FindClientHintsToRemove(nullptr, GURL(), &removed_headers);
  EXPECT_THAT(removed_headers,
              UnorderedElementsAre(
                  "device-memory", "dpr", "width", "viewport-width", "rtt",
                  "downlink", "ect", "sec-ch-lang", "sec-ch-ua-arch",
                  "sec-ch-ua-model", "sec-ch-ua-full-version",
                  "sec-ch-ua-platform-version", "sec-ch-prefers-color-scheme",
                  "sec-ch-ua-bitness", "sec-ch-ua-reduced"));
}
}  // namespace blink
