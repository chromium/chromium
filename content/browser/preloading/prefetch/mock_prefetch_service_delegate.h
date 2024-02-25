// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_MOCK_PREFETCH_SERVICE_DELEGATE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_MOCK_PREFETCH_SERVICE_DELEGATE_H_

#include <optional>

#include "content/public/browser/prefetch_service_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockPrefetchServiceDelegate : public PrefetchServiceDelegate {
 public:
  static const char kPrefetchProxyAddress[];

  explicit MockPrefetchServiceDelegate(
      std::optional<int> num_on_prefetch_likely_calls = 1);

  ~MockPrefetchServiceDelegate() override;

  MockPrefetchServiceDelegate(const MockPrefetchServiceDelegate&) = delete;
  MockPrefetchServiceDelegate& operator=(const MockPrefetchServiceDelegate) =
      delete;

  // PrefetchServiceDelegate.
  MOCK_METHOD(std::string, GetMajorVersionNumber, (), (override));
  MOCK_METHOD(std::string, GetAcceptLanguageHeader, (), (override));
  MOCK_METHOD(GURL, GetDefaultPrefetchProxyHost, (), (override));
  MOCK_METHOD(std::string, GetAPIKey, (), (override));
  MOCK_METHOD(GURL, GetDefaultDNSCanaryCheckURL, (), (override));
  MOCK_METHOD(GURL, GetDefaultTLSCanaryCheckURL, (), (override));
  MOCK_METHOD(void,
              ReportOriginRetryAfter,
              (const GURL&, base::TimeDelta),
              (override));
  MOCK_METHOD(bool, IsOriginOutsideRetryAfterWindow, (const GURL&), (override));
  MOCK_METHOD(void, ClearData, (), (override));
  MOCK_METHOD(bool, DisableDecoysBasedOnUserSettings, (), (override));
  MOCK_METHOD(PreloadingEligibility, IsSomePreloadingEnabled, (), (override));
  MOCK_METHOD(bool, IsExtendedPreloadingEnabled, (), (override));
  MOCK_METHOD(bool, IsPreloadingPrefEnabled, (), (override));
  MOCK_METHOD(bool, IsDataSaverEnabled, (), (override));
  MOCK_METHOD(bool, IsBatterySaverEnabled, (), (override));
  MOCK_METHOD(bool, IsDomainInPrefetchAllowList, (const GURL&), (override));
  MOCK_METHOD(bool, IsContaminationExempt, (const GURL&), (override));
  MOCK_METHOD(void, OnPrefetchLikely, (WebContents*), (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_MOCK_PREFETCH_SERVICE_DELEGATE_H_
