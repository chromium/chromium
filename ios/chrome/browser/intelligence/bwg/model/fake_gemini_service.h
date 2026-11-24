// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_

#import <optional>
#import <string>

#import "base/observer_list.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"

// Fake GeminiService for testing.
class FakeGeminiService : public GeminiService {
 public:
  FakeGeminiService();
  ~FakeGeminiService() override;

  // GeminiService:
  void AddObserver(GeminiService::Observer* observer) override;
  void RemoveObserver(GeminiService::Observer* observer) override;
  bool IsProfileEligibleForGemini() override;
  std::optional<gemini::IneligibilityReasons> GeminiIneligibilityForProfile()
      override;
  bool IsWorkspacePolicyCheckPending() override;
  void CheckGeminiEnterpriseEligibilityIfNeeded() override;
  std::optional<std::string> GetModelQualityClientID() override;

  // Test helpers:
  void SetIsEligible(bool is_eligible) {
    bool changed = (IsProfileEligibleForGemini() != is_eligible);
    if (is_eligible) {
      ineligibility_reasons_ = std::nullopt;
    } else {
      gemini::IneligibilityReasons reasons;
      reasons.chrome_enterprise = true;
      ineligibility_reasons_ = reasons;
    }
    if (changed) {
      for (auto& observer : observers_) {
        observer.OnGeminiEligibilityChanged();
      }
    }
  }

  void SetIneligibilityReasons(
      std::optional<gemini::IneligibilityReasons> reasons) {
    bool changed = false;
    if (ineligibility_reasons_.has_value() != reasons.has_value()) {
      changed = true;
    } else if (ineligibility_reasons_.has_value() && reasons.has_value()) {
      changed =
          ineligibility_reasons_->workspace != reasons->workspace ||
          ineligibility_reasons_->chrome_enterprise !=
              reasons->chrome_enterprise ||
          ineligibility_reasons_->account_capability !=
              reasons->account_capability ||
          ineligibility_reasons_->authentication != reasons->authentication;
    }
    ineligibility_reasons_ = reasons;
    if (changed) {
      for (auto& observer : observers_) {
        observer.OnGeminiEligibilityChanged();
      }
    }
  }

  void SetWorkspacePolicyCheckPending(bool pending) {
    bool changed = (workspace_policy_check_pending_ != pending);
    workspace_policy_check_pending_ = pending;
    if (changed) {
      for (auto& observer : observers_) {
        observer.OnGeminiEligibilityChanged();
      }
    }
  }

  bool HasGeminiInChromeCapability() override;

  void SetHasGeminiInChromeCapability(bool allowed) {
    has_gemini_in_chrome_capability_ = allowed;
  }

  bool HasModelExecutionCapability() override;

  void SetHasModelExecutionCapability(bool allowed) {
    has_model_execution_capability_ = allowed;
  }

 private:
  base::ObserverList<GeminiService::Observer> observers_;
  std::optional<gemini::IneligibilityReasons> ineligibility_reasons_;
  bool workspace_policy_check_pending_ = false;
  bool has_gemini_in_chrome_capability_ = true;
  bool has_model_execution_capability_ = true;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_SERVICE_H_
