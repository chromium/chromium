// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/net/record_load_histograms.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

void RecordLoadHistograms(const GURL& url,
                          ResourceType resource_type,
                          int net_error) {
  // Requests shouldn't complete with net::ERR_IO_PENDING.
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (resource_type == ResourceType::kMainFrame) {
    base::UmaHistogramSparse("Net.ErrorCodesForMainFrame4", -net_error);
    if (url.SchemeIsCryptographic()) {
      if (url.host_piece() == "www.google.com") {
        base::UmaHistogramSparse("Net.ErrorCodesForHTTPSGoogleMainFrame3",
                                 -net_error);
      }

      if (net::IsTLS13ExperimentHost(url.host_piece())) {
        base::UmaHistogramSparse("Net.ErrorCodesForTLS13ExperimentMainFrame2",
                                 -net_error);
      }
    }
  } else {
    if (resource_type == ResourceType::kImage) {
      base::UmaHistogramSparse("Net.ErrorCodesForImages2", -net_error);
    }
    base::UmaHistogramSparse("Net.ErrorCodesForSubresources3", -net_error);
  }
}

}  // namespace content
