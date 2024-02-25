// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_GENERATION_OUTCOME_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_GENERATION_OUTCOME_H_

// Enum representing the set of possible link generation outcomes from the
// text-fragments-polyfill library. To be kept in sync with the
// `GenerateFragmentStatus` enum in that library.
enum class LinkGenerationOutcome {
  kSuccess = 0,
  kInvalidSelection = 1,
  kAmbiguous = 2,
  kTimeout = 3,
  kExecutionFailed = 4,
  kMaxValue = kExecutionFailed
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_GENERATION_OUTCOME_H_
