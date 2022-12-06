// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "remoting/codec/utils.h"

namespace remoting {

webrtc::DesktopRect GetBoundingRect(const webrtc::DesktopRegion& region) {
  webrtc::DesktopRect bound;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    bound.UnionWith(r.rect());
  }
  return bound;
}

}  // namespace remoting
