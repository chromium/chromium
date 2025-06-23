// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_result.h"

namespace content {

PermissionResult::PermissionResult()
    : status(PermissionStatus::ASK),
      source(PermissionStatusSource::UNSPECIFIED) {}

PermissionResult::PermissionResult(
    PermissionStatus permission_status,
    PermissionStatusSource permission_status_source,
    std::optional<base::Value> retrieved_permission_data)
    : status(permission_status),
      source(permission_status_source),
      retrieved_permission_data(std::move(retrieved_permission_data)) {}

PermissionResult::PermissionResult(const PermissionResult& other) {
  status = other.status;
  source = other.source;
  retrieved_permission_data =
      other.retrieved_permission_data.has_value()
          ? std::make_optional(other.retrieved_permission_data.value().Clone())
          : std::nullopt;
}
PermissionResult& PermissionResult::operator=(PermissionResult& other) {
  status = other.status;
  source = other.source;
  retrieved_permission_data = std::move(other.retrieved_permission_data);
  return *this;
}
PermissionResult::PermissionResult(PermissionResult&&) = default;
PermissionResult& PermissionResult::operator=(PermissionResult&&) = default;

PermissionResult::~PermissionResult() = default;

}  // namespace content
