// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"

bool FakeGeminiService::IsProfileEligibleForGemini() {
  return !ineligibility_reasons_.has_value();
}

std::optional<gemini::IneligibilityReasons>
FakeGeminiService::GeminiIneligibilityForProfile() {
  return ineligibility_reasons_;
}
