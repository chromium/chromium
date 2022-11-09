// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_level_attribution_manager_impl.h"

#include "components/attribution_reporting/os_registration.h"

namespace content {

OsLevelAttributionManagerImpl::OsLevelAttributionManagerImpl() = default;

OsLevelAttributionManagerImpl::~OsLevelAttributionManagerImpl() = default;

void OsLevelAttributionManagerImpl::RegisterSource(
    attribution_reporting::OsSource) {
  // TODO(https://crbug.com/1374035): Route this source to an appropriate API
  // provided by the underlying platform.
}

void OsLevelAttributionManagerImpl::RegisterTrigger(
    attribution_reporting::OsTrigger) {
  // TODO(https://crbug.com/1374035): Route this trigger to an appropriate API
  // provided by the underlying platform.
}

}  // namespace content
