// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_exposed_isolation_info.h"

#include <ostream>

namespace content {

// static
WebExposedIsolationInfo WebExposedIsolationInfo::CreateNonIsolated() {
  return WebExposedIsolationInfo(absl::nullopt /* origin */,
                                 false /* isolated_application */);
}

WebExposedIsolationInfo WebExposedIsolationInfo::CreateIsolated(
    const url::Origin& origin) {
  return WebExposedIsolationInfo(origin, false /* isolated_application */);
}

WebExposedIsolationInfo WebExposedIsolationInfo::CreateIsolatedApplication(
    const url::Origin& origin) {
  return WebExposedIsolationInfo(origin, true /* isolated_application */);
}

WebExposedIsolationInfo::WebExposedIsolationInfo(
    const absl::optional<url::Origin>& origin,
    bool isolated_application)
    : origin_(origin), isolated_application_(isolated_application) {}

WebExposedIsolationInfo::WebExposedIsolationInfo(
    const WebExposedIsolationInfo& other) = default;

WebExposedIsolationInfo::~WebExposedIsolationInfo() = default;

const url::Origin& WebExposedIsolationInfo::origin() const {
  DCHECK(is_isolated())
      << "The origin() getter should only be used for "
         "WebExposedIsolationInfo's where is_isolated() is true.";
  return origin_.value();
}

bool WebExposedIsolationInfo::operator==(
    const WebExposedIsolationInfo& b) const {
  if (is_isolated_application() != b.is_isolated_application())
    return false;

  if (is_isolated() != b.is_isolated())
    return false;

  if (is_isolated() && !(origin_->IsSameOriginWith(b.origin())))
    return false;

  return true;
}

bool WebExposedIsolationInfo::operator!=(
    const WebExposedIsolationInfo& b) const {
  return !(operator==(b));
}

bool WebExposedIsolationInfo::operator<(
    const WebExposedIsolationInfo& b) const {
  // Nonisolated < Isolated < Isolated Application.
  if (is_isolated_application() != b.is_isolated_application())
    return !is_isolated_application();

  if (is_isolated() != b.is_isolated())
    return !is_isolated();

  // Within Isolated and Isolated Application, compare origins:
  if (is_isolated()) {
    DCHECK(b.is_isolated());
    return origin_.value() < b.origin();
  }

  // Nonisolated == Nonisolated.
  DCHECK(!is_isolated() && !b.is_isolated());
  return false;
}

std::ostream& operator<<(std::ostream& out,
                         const WebExposedIsolationInfo& info) {
  out << "{";
  if (info.is_isolated()) {
    out << info.origin();
    if (info.is_isolated_application())
      out << " (application)";
  }
  out << "}";
  return out;
}
}  // namespace content
