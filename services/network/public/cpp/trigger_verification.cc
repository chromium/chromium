// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trigger_verification.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/uuid.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

TriggerVerification::TriggerVerification() = default;
TriggerVerification::~TriggerVerification() = default;

TriggerVerification::TriggerVerification(const TriggerVerification&) = default;
TriggerVerification& TriggerVerification::operator=(
    const TriggerVerification&) = default;

TriggerVerification::TriggerVerification(TriggerVerification&&) = default;
TriggerVerification& TriggerVerification::operator=(TriggerVerification&&) =
    default;

// static
absl::optional<TriggerVerification> TriggerVerification::Create(
    std::string token,
    const std::string& aggregatable_report_id) {
  base::Uuid id = base::Uuid::ParseLowercase(aggregatable_report_id);
  if (!id.is_valid() || token.empty()) {
    return absl::nullopt;
  }

  return TriggerVerification(std::move(token), std::move(id));
}

TriggerVerification::TriggerVerification(std::string token,
                                         base::Uuid aggregatable_report_id)
    : token_(std::move(token)),
      aggregatable_report_id_(std::move(aggregatable_report_id)) {
  DCHECK(aggregatable_report_id_.is_valid());
  DCHECK(!token_.empty());
}

}  // namespace network
