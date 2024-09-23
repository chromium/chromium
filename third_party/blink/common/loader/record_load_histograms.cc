// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/record_load_histograms.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace blink {

namespace {
constexpr char kIsolatedAppScheme[] = "isolated-app";
}

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
    base::UmaHistogramSparse("Net.ErrorCodesForSubresources3", -net_error);
  }

  // TODO(crbug.com/1384451): This is a temporary metric for monitoring the
  // launch of Isolated Web Apps over the course of 2023.
  if (origin.scheme() == kIsolatedAppScheme) {
    base::UmaHistogramSparse("Net.ErrorCodesForIsolatedAppScheme", -net_error);
  }
}

}  // namespace blink
