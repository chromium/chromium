// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/evp_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/webid/delegation/email_verification_request.h"

namespace content::webid {

void RecordEvpRequestStatus(EvpRequestStatus status) {
  base::UmaHistogramEnumeration("Blink.Evp.Status.Request", status);
}

}  // namespace content::webid
