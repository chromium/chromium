/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_IMPL_H_

#include <memory>

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

class AudioSourceProvider;
class ImageCapture;
class MediaTrackCapabilities;
class MediaTrackConstraints;
class MediaStream;
class MediaTrackSettings;
class ScriptPromiseResolver;
class ScriptState;

// Primary implementation of the MediaStreamTrack interface and idl type.
class MODULES_EXPORT MediaStreamTrackImpl : public MediaStreamTrack,
                                            public MediaStreamSource::Observer {
 public:
  // Create a MediaStreamTrackImpl of the appropriate type for the display
  // surface type.
  static MediaStreamTrack* Create(ExecutionContext* context,
                                  MediaStreamComponent* component,
                                  base::OnceClosure callback,
                                  const String& descriptor_id);

  MediaStreamTrackImpl(ExecutionContext*, MediaStreamComponent*);
  MediaStreamTrackImpl(ExecutionContext*,
                       MediaStreamComponent*,
                       base::OnceClosure callback);
  MediaStreamTrackImpl(ExecutionContext*,
                       MediaStreamComponent*,
                       MediaStreamSource::ReadyState,
                       base::OnceClosure callback);
  ~MediaStreamTrackImpl() override;

  // MediaStreamTrack
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

  // This function is called when constrains have been successfully applied.
  // Called from UserMediaRequest when it succeeds. It is not IDL-exposed.
  void SetConstraints(const MediaConstraints&) override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(mute, kMute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute, kUnmute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(capturehandlechange, kCapturehandlechange)

  // Returns the enum value of the ready state.
  MediaStreamSource::ReadyState GetReadyState() override {
    return ready_state_;
  }

  MediaStreamComponent* Component() const override { return component_; }
  bool Ended() const override;

  void RegisterMediaStream(MediaStream*) override;
  void UnregisterMediaStream(MediaStream*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void AddedEventListener(const AtomicString&,
                          RegisteredEventListener&) override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  std::unique_ptr<AudioSourceProvider> CreateWebAudioSource(
      int context_sample_rate) override;

  ImageCapture* GetImageCapture() override { return image_capture_; }

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

  void AddObserver(MediaStreamTrack::Observer*) override;

  void Trace(Visitor*) const override;

 protected:
  // Given a partially built MediaStreamTrackImpl, finishes the job of making it
  // into a clone of |this|.
  // Useful for sub-classes, as they need to clone both state from
  // this class as well as of their own class.
  void CloneInternal(MediaStreamTrackImpl*);

 private:
  friend class CanvasCaptureMediaStreamTrack;

  // MediaStreamTrack
  void applyConstraints(ScriptPromiseResolver*,
                        const MediaTrackConstraints*) override;

  // MediaStreamSource::Observer
  void SourceChangedState() override;
  void SourceChangedCaptureHandle() override;

  void PropagateTrackEnded();
  void applyConstraintsImageCapture(ScriptPromiseResolver*,
                                    const MediaTrackConstraints*);

  void SendLogMessage(const WTF::String& message);

  // Ensures that |feature_handle_for_scheduler_| is initialized.
  void EnsureFeatureHandleForScheduler();

  void setReadyState(MediaStreamSource::ReadyState ready_state);

  // This handle notifies the scheduler about a live media stream track
  // associated with a frame. The handle should be destroyed when the track
  // is stopped.
  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  MediaStreamSource::ReadyState ready_state_;
  HeapHashSet<Member<MediaStream>> registered_media_streams_;
  bool is_iterating_registered_media_streams_ = false;
  Member<MediaStreamComponent> component_;
  Member<ImageCapture> image_capture_;
  WeakMember<ExecutionContext> execution_context_;
  HeapHashSet<WeakMember<MediaStreamTrack::Observer>> observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_IMPL_H_
