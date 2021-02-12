// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/coop_coep_cross_origin_isolated_info.h"

namespace content {

// static
CoopCoepCrossOriginIsolatedInfo
CoopCoepCrossOriginIsolatedInfo::CreateNonIsolated() {
  return CoopCoepCrossOriginIsolatedInfo(base::nullopt /* origin */);
}

CoopCoepCrossOriginIsolatedInfo CoopCoepCrossOriginIsolatedInfo::CreateIsolated(
    const url::Origin& origin) {
  return CoopCoepCrossOriginIsolatedInfo(origin);
}

CoopCoepCrossOriginIsolatedInfo::CoopCoepCrossOriginIsolatedInfo(
    const base::Optional<url::Origin>& origin)
    : origin_(origin) {}

CoopCoepCrossOriginIsolatedInfo::CoopCoepCrossOriginIsolatedInfo(
    const CoopCoepCrossOriginIsolatedInfo& other) = default;

CoopCoepCrossOriginIsolatedInfo::~CoopCoepCrossOriginIsolatedInfo() = default;

const url::Origin& CoopCoepCrossOriginIsolatedInfo::origin() const {
  DCHECK(is_isolated())
      << "The origin() getter should only be used for "
         "CoopCoepCrossOriginIsolatedInfo's where is_isolated() is true.";
  return origin_.value();
}

bool CoopCoepCrossOriginIsolatedInfo::operator==(
    const CoopCoepCrossOriginIsolatedInfo& b) const {
  if (is_isolated() != b.is_isolated())
    return false;

  if (is_isolated() && !(origin_->IsSameOriginWith(b.origin())))
    return false;

  return true;
}

bool CoopCoepCrossOriginIsolatedInfo::operator!=(
    const CoopCoepCrossOriginIsolatedInfo& b) const {
  return !(operator==(b));
}

bool CoopCoepCrossOriginIsolatedInfo::operator<(
    const CoopCoepCrossOriginIsolatedInfo& b) const {
  if (!(is_isolated() && b.is_isolated()))
    return is_isolated() < b.is_isolated();

  return origin_.value() < b.origin();
}

std::ostream& operator<<(std::ostream& out,
                         const CoopCoepCrossOriginIsolatedInfo& info) {
  out << "{";
  if (info.is_isolated()) {
    out << info.origin();
  }
  out << "}";
  return out;
}
}  // namespace content
