// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/mock_prefetch_service_delegate.h"

namespace content {

namespace {
const char kApiKey[] = "APIKEY";
}  // namespace

const char MockPrefetchServiceDelegate::kPrefetchProxyAddress[] =
    "https://testprefetchproxy.com";

MockPrefetchServiceDelegate::MockPrefetchServiceDelegate(
    std::optional<int> num_on_prefetch_likely_calls) {
  // Sets default behavior for the delegate.
  ON_CALL(*this, GetDefaultPrefetchProxyHost)
      .WillByDefault(testing::Return(GURL(kPrefetchProxyAddress)));
  ON_CALL(*this, GetAPIKey).WillByDefault(testing::Return(kApiKey));
  ON_CALL(*this, IsOriginOutsideRetryAfterWindow(testing::_))
      .WillByDefault(testing::Return(true));
  ON_CALL(*this, DisableDecoysBasedOnUserSettings)
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, IsSomePreloadingEnabled)
      .WillByDefault(testing::Return(PreloadingEligibility::kEligible));
  ON_CALL(*this, IsExtendedPreloadingEnabled)
      .WillByDefault(testing::Return(false));
  ON_CALL(*this, IsPreloadingPrefEnabled).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsDataSaverEnabled).WillByDefault(testing::Return(false));
  ON_CALL(*this, IsBatterySaverEnabled).WillByDefault(testing::Return(false));
  ON_CALL(*this, IsContaminationExempt).WillByDefault(testing::Return(false));
  ON_CALL(*this, IsDomainInPrefetchAllowList(testing::_))
      .WillByDefault(testing::Return(true));

  if (num_on_prefetch_likely_calls.has_value()) {
    EXPECT_CALL(*this, OnPrefetchLikely(testing::_))
        .Times(num_on_prefetch_likely_calls.value());
  }
}

MockPrefetchServiceDelegate::~MockPrefetchServiceDelegate() = default;

}  // namespace content
