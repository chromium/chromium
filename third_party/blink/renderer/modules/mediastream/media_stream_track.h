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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class AudioSourceProvider;
class ImageCapture;
class MediaTrackCapabilities;
class MediaTrackConstraints;
class MediaStream;
class MediaTrackSettings;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT MediaStreamTrack
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaStreamTrack>,
      public MediaStreamSource::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(MediaStreamTrack);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStreamTrack* Create(ExecutionContext*, MediaStreamComponent*);

  MediaStreamTrack(ExecutionContext*, MediaStreamComponent*);
  MediaStreamTrack(ExecutionContext*,
                   MediaStreamComponent*,
                   MediaStreamSource::ReadyState);
  ~MediaStreamTrack() override;

  String kind() const;
  String id() const;
  String label() const;

  bool enabled() const;
  void setEnabled(bool);

  bool muted() const;

  String ContentHint() const;
  void SetContentHint(const String&);

  String readyState() const;

  void stopTrack(ExecutionContext*);
  virtual MediaStreamTrack* clone(ScriptState*);

  // This function is called when constrains have been successfully applied.
  // Called from UserMediaRequest when it succeeds. It is not IDL-exposed.
  void SetConstraints(const WebMediaConstraints&);

  MediaTrackCapabilities* getCapabilities() const;
  MediaTrackConstraints* getConstraints() const;
  MediaTrackSettings* getSettings() const;
  ScriptPromise applyConstraints(ScriptState*, const MediaTrackConstraints*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(mute, kMute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute, kUnmute)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(ended, kEnded)

  MediaStreamComponent* Component() { return component_; }
  bool Ended() const;

  void RegisterMediaStream(MediaStream*);
  void UnregisterMediaStream(MediaStream*);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  std::unique_ptr<AudioSourceProvider> CreateWebAudioSource(
      int context_sample_rate);

  void Trace(blink::Visitor*) override;

 private:
  friend class CanvasCaptureMediaStreamTrack;

  // MediaStreamSourceObserver
  void SourceChangedState() override;

  void PropagateTrackEnded();
  void applyConstraintsImageCapture(ScriptPromiseResolver*,
                                    const MediaTrackConstraints*);

  MediaStreamSource::ReadyState ready_state_;
  HeapHashSet<Member<MediaStream>> registered_media_streams_;
  bool is_iterating_registered_media_streams_ = false;
  Member<MediaStreamComponent> component_;
  Member<ImageCapture> image_capture_;
  WeakMember<ExecutionContext> execution_context_;
};

typedef HeapVector<Member<MediaStreamTrack>> MediaStreamTrackVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_TRACK_H_
