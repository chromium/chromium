// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stream_test_utils.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

namespace blink {

MockMediaStreamVideoSource* CreateMockVideoSource() {
  MockMediaStreamVideoSource* mock_video_source =
      new MockMediaStreamVideoSource();
  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "source_id", MediaStreamSource::kTypeVideo, "source_name",
          /*remote=*/false);
  media_stream_source->SetPlatformSource(base::WrapUnique(mock_video_source));
  return mock_video_source;
}

MediaStreamTrack* CreateVideoMediaStreamTrack(ExecutionContext* context,
                                              MediaStreamVideoSource* source) {
  return MakeGarbageCollected<MediaStreamTrack>(
      context, MediaStreamVideoTrack::CreateVideoTrack(
                   source, MediaStreamVideoSource::ConstraintsOnceCallback(),
                   /*enabled=*/true));
}

}  // namespace blink
