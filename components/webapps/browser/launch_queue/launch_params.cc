// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/launch_queue/launch_params.h"

namespace webapps {

LaunchParams::LaunchParams() = default;

LaunchParams::LaunchParams(const LaunchParams&) = default;

LaunchParams::LaunchParams(LaunchParams&&) noexcept = default;

LaunchParams::~LaunchParams() = default;

LaunchParams& LaunchParams::operator=(const LaunchParams&) = default;

LaunchParams& LaunchParams::operator=(LaunchParams&&) noexcept = default;

}  // namespace webapps
