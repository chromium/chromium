// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TESTING_INTERNALS_MEDIA_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TESTING_INTERNALS_MEDIA_STREAM_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Internals;
class MediaDeviceInfo;
class MediaStreamTrack;
class MediaTrackConstraints;
class ScriptPromise;
class ScriptState;

class InternalsMediaStream {
  STATIC_ONLY(InternalsMediaStream);

 public:
  static ScriptPromise addFakeDevice(ScriptState*,
                                     Internals&,
                                     const MediaDeviceInfo*,
                                     const MediaTrackConstraints* capabilities,
                                     const MediaStreamTrack* data_source);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TESTING_INTERNALS_MEDIA_STREAM_H_
