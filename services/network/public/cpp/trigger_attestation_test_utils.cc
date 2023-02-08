// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trigger_attestation.h"

#include <ostream>

namespace network {

bool operator==(const TriggerAttestation&& a, const TriggerAttestation&& b) {
  auto tie = [](const TriggerAttestation& t) {
    return std::make_tuple(t.token(), t.aggregatable_report_id());
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         const TriggerAttestation& attestation) {
  return out << "{token=" << attestation.token() << ",aggregatable_report_id="
             << attestation.aggregatable_report_id() << "}";
}

}  // namespace network
