// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_METRICS_RECORDER_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_METRICS_RECORDER_H_

#include "base/time/time.h"
#include "services/network/attribution/attribution_attestation_mediator.h"

namespace network {

// A `AttributionAttestationMediatorMetricsRecorder` records timing and status
// metrics for a single attestation operation.
class AttributionAttestationMediatorMetricsRecorder
    : public AttributionAttestationMediator::MetricsRecorder {
 public:
  AttributionAttestationMediatorMetricsRecorder();
  ~AttributionAttestationMediatorMetricsRecorder() override;

  void Start() override;
  void Complete(AttributionAttestationMediator::Step step) override;
  void FinishGetHeadersWith(
      AttributionAttestationMediator::GetHeadersStatus status) override;
  void FinishProcessAttestationWith(
      AttributionAttestationMediator::ProcessAttestationStatus status) override;

 private:
  base::TimeTicks get_headers_start_;

  base::TimeTicks get_key_commitment_end_;
  base::TimeTicks crypto_initialization_end_;
  base::TimeTicks blind_message_end_;

  base::TimeTicks sign_blind_message_end_;
  base::TimeTicks unblind_message_end_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_ATTESTATION_MEDIATOR_METRICS_RECORDER_H_
