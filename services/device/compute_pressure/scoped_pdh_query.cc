// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/scoped_pdh_query.h"

namespace device {

ScopedPdhQuery::ScopedPdhQuery() = default;

ScopedPdhQuery::ScopedPdhQuery(PDH_HQUERY pdh_query)
    : ScopedGeneric(pdh_query) {}

// static
ScopedPdhQuery ScopedPdhQuery::Create() {
  PDH_HQUERY pdh_query;
  PDH_STATUS pdh_status = PdhOpenQuery(NULL, NULL, &pdh_query);
  if (pdh_status == ERROR_SUCCESS)
    return ScopedPdhQuery(std::move(pdh_query));
  return ScopedPdhQuery();
}

}  // namespace device
