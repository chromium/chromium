// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_RESPONSE_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_RESPONSE_H_

#import <string_view>

#import "base/types/expected.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"

// Represents possible errors while parsing response from Omaha.
enum OmahaParsingError {
  kServerError,
  kInvalidXML,
  kInvalidResponse,
};

// Represents a successfully parsed response from Omaha server.
struct OmahaResponse {
  int server_date = 0;
  std::optional<UpgradeRecommendedDetails> details;

  friend constexpr bool operator==(const OmahaResponse&,
                                   const OmahaResponse&) = default;
};

// Try to parse an Omaha server response, returning nullopt in case of failure.
base::expected<OmahaResponse, OmahaParsingError> ParseOmahaResponse(
    std::string_view application_id,
    std::string_view response_body);

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_RESPONSE_H_
