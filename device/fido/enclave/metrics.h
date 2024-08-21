// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_METRICS_H_
#define DEVICE_FIDO_ENCLAVE_METRICS_H_

#include "base/component_export.h"

namespace device::enclave {

// A list of enclave-related events that are reported to UMA. Do not renumber
// as the values are persisted.
enum class Event {
  kOnboarding = 0,
  kOnboardingRejected = 1,
  kOnboardingAccepted = 2,
  kRecoveryShown = 3,
  kRecoverySuccessful = 4,
  kGetAssertion = 5,
  kMakeCredential = 6,
  kMakeCredentialPriorityShown = 7,
  kMakeCredentialPriorityDeclined = 8,
  kICloudRecoverySuccessful = 9,

  kMaxValue = 9,
};

COMPONENT_EXPORT(DEVICE_FIDO) void RecordEvent(Event event);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_METRICS_H_
