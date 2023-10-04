// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_CONSTANTS_H_

// Enum for the seed fetch result histogram. Must stay in sync with
// `VariationsSeedFetchResult` from enums.xml.
enum class IOSSeedFetchException : int {
  // Default value. DO NOT LOG.
  kNotApplicable = 0,
  // HTTPS request times out.
  kHTTPSRequestTimeout = -2,
  // Variations URL error.
  kHTTPSRequestBadUrl = -3,
  // The "IM" header returned from the variations server does not exist or
  // contains invalid value.
  kInvalidIMHeader = -5,
};

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_CONSTANTS_H_
