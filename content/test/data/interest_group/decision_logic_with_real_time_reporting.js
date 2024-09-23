// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  realTimeReporting.contributeToHistogram(
      {bucket: 200 + bid, priorityWeight: 1.5});
  return bid;
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
}
