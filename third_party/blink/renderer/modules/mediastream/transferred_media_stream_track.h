// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaTrackCapabilities;
class MediaTrackConstraints;
class MediaTrackSettings;
class ScriptState;

// A MediaStreamTrack implementation synchronously created when receiving a
// transferred MediaStreamTrack, when the full instance is being asynchronously
// created. Once the asynchronous setup has finished, proxies all calls to the
// full instance.
class MODULES_EXPORT TransferredMediaStreamTrack : public MediaStreamTrack {
 public:
  explicit TransferredMediaStreamTrack(ExecutionContext*);

  // MediaStreamTrack.idl
  String kind() const override;
  String id() const override;
  String label() const override;
  bool enabled() const override;
  void setEnabled(bool) override;
  bool muted() const override;
  String ContentHint() const override;
  void SetContentHint(const String&) override;
  String readyState() const override;
  MediaStreamTrack* clone(ScriptState*) override;
  void stopTrack(ExecutionContext*) override;
  MediaTrackCapabilities* getCapabilities() const override;
  MediaTrackConstraints* getConstraints() const override;
  MediaTrackSettings* getSettings() const override;
  CaptureHandle* getCaptureHandle() const override;
  ScriptPromise applyConstraints(ScriptState*,
                                 const MediaTrackConstraints*) override;

  void setImplementation(MediaStreamTrack* track);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(mute, kMute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute, kUnmute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(capturehandlechange, kCapturehandlechange)

  void Trace(Visitor*) const override;

 private:
  Member<MediaStreamTrack> track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_
