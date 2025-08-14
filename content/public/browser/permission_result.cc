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
    std::optional<PermissionSetting> retrieved_permission_setting)
    : status(permission_status),
      source(permission_status_source),
      retrieved_permission_setting(retrieved_permission_setting) {}

PermissionResult::PermissionResult(const PermissionResult& other) {
  status = other.status;
  source = other.source;
  retrieved_permission_setting = other.retrieved_permission_setting;
}

PermissionResult& PermissionResult::operator=(PermissionResult& other) =
    default;
PermissionResult::PermissionResult(PermissionResult&&) = default;
PermissionResult& PermissionResult::operator=(PermissionResult&&) = default;

PermissionResult::~PermissionResult() = default;

bool PermissionResult::operator==(const PermissionResult& rhs) const = default;

}  // namespace content
