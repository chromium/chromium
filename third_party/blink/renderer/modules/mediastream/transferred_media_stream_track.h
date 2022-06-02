// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
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
  TransferredMediaStreamTrack(ExecutionContext* execution_context,
                              const TransferredValues& data);

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

  bool HasImplementation() const { return !!track_; }
  void SetImplementation(MediaStreamTrack* track);

  void SetConstraints(const MediaConstraints&) override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(mute, kMute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute, kUnmute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(capturehandlechange, kCapturehandlechange)

  MediaStreamSource::ReadyState GetReadyState() override;

  MediaStreamComponent* Component() const override;
  bool Ended() const override;

  void RegisterMediaStream(MediaStream*) override;
  void UnregisterMediaStream(MediaStream*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void AddedEventListener(const AtomicString&,
                          RegisteredEventListener&) override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  std::unique_ptr<AudioSourceProvider> CreateWebAudioSource(
      int context_sample_rate) override;

  ImageCapture* GetImageCapture() override;
  absl::optional<base::UnguessableToken> serializable_session_id()
      const override;

#if !BUILDFLAG(IS_ANDROID)
  // Only relevant for focusable streams (FocusableMediaStreamTrack).
  // When called on one of these, it signals that Conditional Focus
  // no longer applies - the browser will now decide whether
  // the captured display surface should be captured. Later calls to
  // FocusableMediaStreamTrack.focus() will now raise an exception.
  void CloseFocusWindowOfOpportunity() override;
#endif

  void AddObserver(Observer*) override;

  void Trace(Visitor*) const override;

 private:
  void applyConstraints(ScriptPromiseResolver*,
                        const MediaTrackConstraints*) override;

  // Helper class to register as an event listener on the underlying
  // MediaStreamTrack and re-dispatch any fired events on the wrapping
  // TransferredMediaStreamTrack.
  class EventPropagator : public NativeEventListener {
   public:
    EventPropagator(MediaStreamTrack* underlying_track,
                    TransferredMediaStreamTrack* transferred_track);
    void Invoke(ExecutionContext*, Event* event) override;
    void Trace(Visitor*) const override;

   private:
    Member<TransferredMediaStreamTrack> transferred_track_;
  };

  Member<MediaStreamTrack> track_;
  using ConstraintsPair =
      std::pair<ScriptPromiseResolver*, const MediaTrackConstraints*>;
  Vector<ConstraintsPair> constraints_list_;
  WeakMember<ExecutionContext> execution_context_;
  TransferredValues data_;
  Member<EventPropagator> event_propagator_;
  HeapHashSet<WeakMember<MediaStreamTrack::Observer>> observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_TRANSFERRED_MEDIA_STREAM_TRACK_H_
