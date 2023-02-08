// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trigger_attestation.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/guid.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

TriggerAttestation::TriggerAttestation() = default;
TriggerAttestation::~TriggerAttestation() = default;

TriggerAttestation::TriggerAttestation(const TriggerAttestation&) = default;
TriggerAttestation& TriggerAttestation::operator=(const TriggerAttestation&) =
    default;

TriggerAttestation::TriggerAttestation(TriggerAttestation&&) = default;
TriggerAttestation& TriggerAttestation::operator=(TriggerAttestation&&) =
    default;

// static
absl::optional<TriggerAttestation> TriggerAttestation::Create(
    std::string token,
    const std::string& aggregatable_report_id) {
  base::GUID id = base::GUID::ParseLowercase(aggregatable_report_id);
  if (!id.is_valid() || token.empty()) {
    return absl::nullopt;
  }

  return TriggerAttestation(std::move(token), std::move(id));
}

TriggerAttestation::TriggerAttestation(std::string token,
                                       base::GUID aggregatable_report_id)
    : token_(std::move(token)),
      aggregatable_report_id_(std::move(aggregatable_report_id)) {
  DCHECK(aggregatable_report_id_.is_valid());
  DCHECK(!token_.empty());
}

}  // namespace network
