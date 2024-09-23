// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/metrics.h"

#include "base/metrics/histogram_functions.h"

namespace device::enclave {

void RecordEvent(Event event) {
  base::UmaHistogramEnumeration("WebAuthentication.EnclaveEvent", event);
}

}  // namespace device::enclave
