// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/attribution_reporting.h"

namespace content {

// static
const AttributionRandomizedResponseRates
    AttributionRandomizedResponseRates::kDefault = {
        .navigation = .0024,
        .event = .0000025,
};

}  // namespace content
