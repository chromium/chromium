// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"

namespace blink {

TEST(OriginTrialsSettingsProviderTest, UnsetSettingsReturnsNullSettings) {
  blink::mojom::OriginTrialsSettingsPtr expected_result(nullptr);
  auto actual_result = OriginTrialsSettingsProvider::Get()->GetSettings();
  EXPECT_EQ(actual_result, expected_result);
  EXPECT_TRUE(actual_result.is_null());
}

TEST(OriginTrialsSettingsProviderTest, ReturnsSettingsThatWereSet) {
  blink::mojom::OriginTrialsSettingsPtr expected_result =
      blink::mojom::OriginTrialsSettings::New();
  expected_result->disabled_tokens = {"token a", "token b"};
  OriginTrialsSettingsProvider::Get()->SetSettings(expected_result.Clone());
  auto actual_result = OriginTrialsSettingsProvider::Get()->GetSettings();
  EXPECT_FALSE(actual_result.is_null());
  EXPECT_EQ(actual_result, expected_result);
}

}  // namespace blink
