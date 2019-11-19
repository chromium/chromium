// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/testing/internals_media_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

namespace blink {

ScriptPromise InternalsMediaStream::addFakeDevice(
    ScriptState* script_state,
    Internals&,
    const MediaDeviceInfo* device_info,
    const MediaTrackConstraints*,
    const MediaStreamTrack* data_source) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Reject();
  return promise;
}

}  // namespace blink
