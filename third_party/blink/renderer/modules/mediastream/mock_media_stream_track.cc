// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"

#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {
const AtomicString& MockMediaStreamTrack::InterfaceName() const {
  static AtomicString interface_name_("MockMediaStreamTrack");
  return interface_name_;
}

}  // namespace blink
