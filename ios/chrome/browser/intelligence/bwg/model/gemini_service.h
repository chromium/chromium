// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_H_

#import <optional>

#import "components/keyed_service/core/keyed_service.h"

namespace gemini {
struct IneligibilityReasons;
}  // namespace gemini

// Interface for GeminiService.
class GeminiService : public KeyedService {
 public:
  ~GeminiService() override = default;

  // Returns whether the current profile is eligible for Gemini.
  virtual bool IsProfileEligibleForGemini() = 0;

  // Provides more information than `IsProfileEligibleForGemini` for cases where
  // you need to know the ineligibility reasons of the profile. Profiles which
  // are deemed eligible for Gemini will result in std::nullopt.
  virtual std::optional<gemini::IneligibilityReasons>
  GeminiIneligibilityForProfile() = 0;

  // Returns whether the async workspace policy check is still in flight.
  // When true, GeminiIneligibilityForProfile() may return stale or
  // incomplete workspace data.
  virtual bool IsWorkspacePolicyCheckPending() = 0;

  // Triggers the workspace policy check if it was never started and no
  // check is currently in flight. Called when the eligibility result is
  // about to be needed (when the Page Action Menu opens).
  virtual void CheckGeminiEnterpriseEligibilityIfNeeded() = 0;

  // Returns whether the account capabilities permit using Gemini in Chrome.
  virtual bool HasGeminiInChromeCapability() = 0;

  // Returns whether the account capabilities permit standard model execution
  // features.
  virtual bool HasModelExecutionCapability() = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SERVICE_H_
