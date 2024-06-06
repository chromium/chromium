// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_REAL_TIME_REPORTING_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_REAL_TIME_REPORTING_H_

namespace auction_worklet {

enum RealTimeReportingPlatformError {
  kBiddingScriptLoadFailure = 0,
  kScoringScriptLoadFailure = 1,
  kTrustedBiddingSignalsFailure = 2,
  kTrustedScoringSignalsFailure = 3,

  kNumValues,
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_REAL_TIME_REPORTING_H_
