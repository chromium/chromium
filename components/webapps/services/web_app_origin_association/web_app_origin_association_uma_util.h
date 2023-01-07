// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_UMA_UTIL_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_UMA_UTIL_H_

namespace webapps {

class WebAppOriginAssociationMetrics {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchResult {
    kFetchSucceed = 0,
    kFetchFailedNoResponseBody = 1,
    kFetchFailedInvalidUrl = 2,
    kMaxValue = kFetchFailedInvalidUrl,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ParseResult {
    kParseSucceeded = 0,
    kParseFailedNotADictionary = 1,
    kParseFailedInvalidJson = 2,
    kMaxValue = kParseFailedInvalidJson,
  };

  static void RecordFetchResult(FetchResult result);
  static void RecordParseResult(ParseResult result);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_UMA_UTIL_H_
