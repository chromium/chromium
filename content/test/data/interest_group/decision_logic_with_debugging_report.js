// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  forDebuggingOnly.reportAdAuctionLoss(
      auctionConfig.seller + '/echo?seller_debug_report_loss/' + adMetadata);
  forDebuggingOnly.reportAdAuctionWin(
      auctionConfig.seller + '/echo?seller_debug_report_win/' + adMetadata);
  return bid;
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
}
