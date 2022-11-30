// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/overlay_info.h"

namespace media {

OverlayInfo::OverlayInfo() = default;
OverlayInfo::OverlayInfo(const OverlayInfo&) = default;
OverlayInfo& OverlayInfo::operator=(const OverlayInfo&) = default;

bool OverlayInfo::HasValidRoutingToken() const {
  return routing_token.has_value();
}

bool OverlayInfo::RefersToSameOverlayAs(const OverlayInfo& other) {
  return routing_token == other.routing_token;
}

}  // namespace media
