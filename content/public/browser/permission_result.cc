// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_result.h"

namespace content {

PermissionResult::PermissionResult(
    PermissionStatus permission_status,
    PermissionStatusSource permission_status_source)
    : status(permission_status), source(permission_status_source) {}

PermissionResult::~PermissionResult() = default;

}  // namespace content
