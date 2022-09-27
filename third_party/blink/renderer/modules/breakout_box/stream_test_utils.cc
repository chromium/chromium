// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stream_test_utils.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

MediaStreamTrack* CreateVideoMediaStreamTrack(ExecutionContext* context,
                                              MediaStreamVideoSource* source) {
  return MakeGarbageCollected<MediaStreamTrackImpl>(
      context, MediaStreamVideoTrack::CreateVideoTrack(
                   source, MediaStreamVideoSource::ConstraintsOnceCallback(),
                   /*enabled=*/true));
}

}  // namespace blink
