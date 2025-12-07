// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_policy_enforcer.h"

#include "net/cert/ct_policy_status.h"

namespace net {

ct::CTPolicyCompliance DefaultCTPolicyEnforcer::CheckCompliance(
    X509Certificate* cert,
    const ct::SCTList& verified_scts,
    base::Time current_time,
    const NetLogWithSource& net_log) const {
  return ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY;
}

std::optional<base::Time> DefaultCTPolicyEnforcer::GetLogDisqualificationTime(
    std::string_view log_id) const {
  return std::nullopt;
}

bool DefaultCTPolicyEnforcer::IsCtEnabled() const {
  return false;
}

}  // namespace net
