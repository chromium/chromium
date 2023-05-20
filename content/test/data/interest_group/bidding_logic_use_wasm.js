// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A generate bid script that offers a bid of 2 passed through a "multiply by 4"
// WASM helper, using the first ad's `renderURL`.
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  const instance = new WebAssembly.Instance(browserSignals.wasmHelper);
  const ad = interestGroup.ads[0];
  const bid = instance.exports.compute(2);
  // The WASM is expected to multiply by 4, so bid should be 2 * 4 = 8.
  if (bid != 8)
    throw 'WASM didn\'t do what was expected:' + bid;
  let result = {'ad': ad, 'bid': bid, 'render': ad.renderURL};
  return result;
}

function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echoall?report_bidder');
}
