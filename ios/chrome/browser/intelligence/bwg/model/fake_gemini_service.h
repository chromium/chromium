// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_

#import <optional>

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"

// Fake GeminiService for testing.
class FakeGeminiService : public GeminiService {
 public:
  FakeGeminiService() = default;
  ~FakeGeminiService() override = default;

  // BwgService:
  bool IsProfileEligibleForGemini() override;
  std::optional<gemini::IneligibilityReasons> GeminiIneligibilityForProfile()
      override;

  // Test helpers:
  void SetIsEligible(bool is_eligible) {
    if (is_eligible) {
      ineligibility_reasons_ = std::nullopt;
    } else {
      gemini::IneligibilityReasons reasons;
      reasons.chrome_enterprise = true;
      ineligibility_reasons_ = reasons;
    }
  }

  void SetIneligibilityReasons(
      std::optional<gemini::IneligibilityReasons> reasons) {
    ineligibility_reasons_ = reasons;
  }

 private:
  std::optional<gemini::IneligibilityReasons> ineligibility_reasons_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_
