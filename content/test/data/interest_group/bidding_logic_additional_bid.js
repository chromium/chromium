// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner +
      '/echoall?report_bidder_regular');
}

function reportAdditionalBidWin(auctionSignals, perBuyerSignals, sellerSignals,
                                browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner +
      '/echoall?report_bidder_additional');
}
