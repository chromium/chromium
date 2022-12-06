// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_UTILS_H_
#define REMOTING_CODEC_UTILS_H_

#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// Gets the bounding rectangle of the provided desktop region.
webrtc::DesktopRect GetBoundingRect(const webrtc::DesktopRegion& region);

}  // namespace remoting

#endif  // REMOTING_CODEC_UTILS_H_
