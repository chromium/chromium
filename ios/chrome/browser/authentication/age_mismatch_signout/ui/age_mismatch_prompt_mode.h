// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_PROMPT_MODE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_PROMPT_MODE_H_

// Mode for the Age Mismatch prompt variations.
// LINT.IfChange(AgeMismatchPromptMode)
enum class AgeMismatchPromptMode {
  // Age mismatch signout outside of sign-in flows.
  kStandard = 0,
  // Age mismatch signout during sign-in flows.
  kSigninFlow = 1,
  kMaxValue = kSigninFlow,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AgeMismatchPromptMode)

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_UI_AGE_MISMATCH_PROMPT_MODE_H_
