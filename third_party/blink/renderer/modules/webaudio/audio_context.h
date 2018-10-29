// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_

#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AudioContextOptions;
class AudioTimestamp;
class Document;
class ExceptionState;
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class ScriptState;
class WebAudioLatencyHint;

// This is an BaseAudioContext which actually plays sound, unlike an
// OfflineAudioContext which renders sound into a buffer.
class MODULES_EXPORT AudioContext : public BaseAudioContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioContext* Create(Document&,
                              const AudioContextOptions&,
                              ExceptionState&);

  ~AudioContext() override;
  void Trace(blink::Visitor*) override;

  // For ContextLifeCycleObserver
  void ContextDestroyed(ExecutionContext*) final;

  ScriptPromise closeContext(ScriptState*);
  bool IsContextClosed() const final;

  ScriptPromise suspendContext(ScriptState*);
  ScriptPromise resumeContext(ScriptState*);

  bool HasRealtimeConstraint() final { return true; }

  void getOutputTimestamp(ScriptState*, AudioTimestamp&);
  double baseLatency() const;

  MediaElementAudioSourceNode* createMediaElementSource(HTMLMediaElement*,
                                                        ExceptionState&);
  MediaStreamAudioSourceNode* createMediaStreamSource(MediaStream*,
                                                      ExceptionState&);
  MediaStreamAudioDestinationNode* createMediaStreamDestination(
      ExceptionState&);

  // Called by handlers of AudioScheduledSourceNode and AudioBufferSourceNode to
  // notify their associated AudioContext when start() is called. It may resume
  // the AudioContext if it is now allowed to start.
  void NotifySourceNodeStart() final;

 protected:
  AudioContext(Document&, const WebAudioLatencyHint&);
  void Uninitialize() final;

 private:
  friend class AudioContextAutoplayTest;

  // Do not change the order of this enum, it is used for metrics.
  enum AutoplayStatus {
    // The AudioContext failed to activate because of user gesture requirements.
    kAutoplayStatusFailed = 0,
    // Same as AutoplayStatusFailed but start() on a node was called with a user
    // gesture.
    // This value is no longer used but the enum entry should not be re-used
    // because it is used for metrics.
    // kAutoplayStatusFailedWithStart = 1,
    // The AudioContext had user gesture requirements and was able to activate
    // with a user gesture.
    kAutoplayStatusSucceeded = 2,

    // Keep at the end.
    kAutoplayStatusCount
  };

  // Returns the AutoplayPolicy currently applying to this instance.
  AutoplayPolicy::Type GetAutoplayPolicy() const;

  // Returns whether the autoplay requirements are fulfilled.
  bool AreAutoplayRequirementsFulfilled() const;

  // Do not change the order of this enum, it is used for metrics.
  enum class AutoplayUnlockType {
    kContextConstructor = 0,
    kContextResume = 1,
    kSourceNodeStart = 2,
    kCount
  };

  // If possible, allows autoplay for the AudioContext and marke it as allowed
  // by the given type.
  void MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType);

  // Returns whether the AudioContext is allowed to start rendering.
  bool IsAllowedToStart() const;

  // Record the current autoplay metrics.
  void RecordAutoplayMetrics();

  void StopRendering();

  void DidClose();

  // Send notification to browser that an AudioContext has started or stopped
  // playing audible audio.
  void NotifyAudibleAudioStarted() final;
  void NotifyAudibleAudioStopped() final;

  unsigned context_id_;
  Member<ScriptPromiseResolver> close_resolver_;

  // Whether a user gesture is required to start this AudioContext.
  bool user_gesture_required_ = false;

  // Autoplay status associated with this AudioContext, if any.
  // Will only be set if there is an autoplay policy in place.
  // Will never be set for OfflineAudioContext.
  base::Optional<AutoplayStatus> autoplay_status_;

  // Autoplay unlock type for this AudioContext.
  // Will only be set if there is an autoplay policy in place.
  // Will never be set for OfflineAudioContext.
  base::Optional<AutoplayUnlockType> autoplay_unlock_type_;

  // Records if start() was ever called for any source node in this context.
  bool source_node_started_ = false;

  // AudioContextManager for reporting audibility.
  mojom::blink::AudioContextManagerPtr audio_context_manager_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
