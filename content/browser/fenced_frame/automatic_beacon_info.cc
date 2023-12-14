// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/automatic_beacon_info.h"

namespace content {

AutomaticBeaconInfo::AutomaticBeaconInfo() = default;

AutomaticBeaconInfo::AutomaticBeaconInfo(
    const std::string& data,
    const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
    bool once,
    bool cross_origin_exposed)
    : data(data),
      destinations(destinations),
      once(once),
      cross_origin_exposed(cross_origin_exposed) {}

AutomaticBeaconInfo::AutomaticBeaconInfo(const AutomaticBeaconInfo&) = default;

AutomaticBeaconInfo::AutomaticBeaconInfo(AutomaticBeaconInfo&&) = default;

AutomaticBeaconInfo& AutomaticBeaconInfo::operator=(
    const AutomaticBeaconInfo&) = default;

AutomaticBeaconInfo& AutomaticBeaconInfo::operator=(AutomaticBeaconInfo&&) =
    default;

AutomaticBeaconInfo::~AutomaticBeaconInfo() = default;

}  // namespace content
