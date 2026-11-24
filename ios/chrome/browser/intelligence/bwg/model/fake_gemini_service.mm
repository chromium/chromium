// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"

FakeGeminiService::FakeGeminiService() = default;
FakeGeminiService::~FakeGeminiService() = default;

void FakeGeminiService::AddObserver(GeminiService::Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeGeminiService::RemoveObserver(GeminiService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeGeminiService::IsProfileEligibleForGemini() {
  return !ineligibility_reasons_.has_value();
}

std::optional<gemini::IneligibilityReasons>
FakeGeminiService::GeminiIneligibilityForProfile() {
  return ineligibility_reasons_;
}

bool FakeGeminiService::IsWorkspacePolicyCheckPending() {
  return workspace_policy_check_pending_;
}

void FakeGeminiService::CheckGeminiEnterpriseEligibilityIfNeeded() {
  // Do nothing in the fake service.
}

bool FakeGeminiService::HasGeminiInChromeCapability() {
  return has_gemini_in_chrome_capability_;
}

bool FakeGeminiService::HasModelExecutionCapability() {
  return has_model_execution_capability_;
}

std::optional<std::string> FakeGeminiService::GetModelQualityClientID() {
  return std::nullopt;
}
