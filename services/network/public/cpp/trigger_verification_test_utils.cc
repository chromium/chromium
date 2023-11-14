// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trigger_verification.h"

#include <ostream>

namespace network {

std::ostream& operator<<(std::ostream& out,
                         const TriggerVerification& verification) {
  return out << "{token=" << verification.token() << ",aggregatable_report_id="
             << verification.aggregatable_report_id() << "}";
}

}  // namespace network
