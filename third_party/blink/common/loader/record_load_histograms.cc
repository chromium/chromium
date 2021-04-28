// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/record_load_histograms.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace blink {

void RecordLoadHistograms(const url::Origin& origin,
                          network::mojom::RequestDestination destination,
                          int net_error) {
  // Requests shouldn't complete with net::ERR_IO_PENDING.
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (destination == network::mojom::RequestDestination::kDocument) {
    base::UmaHistogramSparse("Net.ErrorCodesForMainFrame4", -net_error);
    if (GURL::SchemeIsCryptographic(origin.scheme()) &&
        origin.host() == "www.google.com") {
      base::UmaHistogramSparse("Net.ErrorCodesForHTTPSGoogleMainFrame3",
                               -net_error);
    }
  } else {
    if (destination == network::mojom::RequestDestination::kImage) {
      base::UmaHistogramSparse("Net.ErrorCodesForImages2", -net_error);
    }
    base::UmaHistogramSparse("Net.ErrorCodesForSubresources3", -net_error);
  }
}

}  // namespace blink
