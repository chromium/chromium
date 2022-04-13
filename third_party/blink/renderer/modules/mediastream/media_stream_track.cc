// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"

namespace blink {

// static
MediaStreamTrack* MediaStreamTrack::Create(ExecutionContext*,
                                           const base::UnguessableToken&) {
  return MakeGarbageCollected<TransferredMediaStreamTrack>();
}

}  // namespace blink
