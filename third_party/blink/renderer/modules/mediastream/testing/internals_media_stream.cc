// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/testing/internals_media_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"

namespace blink {

ScriptPromise<IDLUndefined> InternalsMediaStream::addFakeDevice(
    ScriptState* script_state,
    Internals&,
    const MediaDeviceInfo* device_info,
    const MediaTrackConstraints*,
    const MediaStreamTrack* data_source) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  resolver->Reject();
  return promise;
}

void InternalsMediaStream::fakeCaptureConfigurationChanged(
    Internals&,
    MediaStreamTrack* track) {
  DCHECK(track);
  auto* video_track = static_cast<MediaStreamTrackImpl*>(track);
  video_track->SourceChangedCaptureConfiguration();
}

}  // namespace blink
