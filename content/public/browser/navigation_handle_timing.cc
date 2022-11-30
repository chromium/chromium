// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_handle_timing.h"

namespace content {

NavigationHandleTiming::NavigationHandleTiming() = default;

NavigationHandleTiming::NavigationHandleTiming(
    const NavigationHandleTiming& timing) = default;

NavigationHandleTiming& NavigationHandleTiming::operator=(
    const NavigationHandleTiming& timing) = default;

}  // namespace content
