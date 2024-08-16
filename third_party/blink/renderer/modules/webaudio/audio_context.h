// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-blink.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiosinkinfo_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiosinkoptions_string.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/setsinkid_resolver.h"
#include "third_party/blink/renderer/platform/audio/audio_frame_stats_accumulator.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AudioContextOptions;
class AudioTimestamp;
class AudioPlayoutStats;
class ExceptionState;
class ExecutionContext;
class HTMLMediaElement;
class LocalDOMWindow;
class MediaElementAudioSourceNode;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class RealtimeAudioDestinationNode;
class ScriptState;
class WebAudioLatencyHint;

// This is an BaseAudioContext which actually plays sound, unlike an
// OfflineAudioContext which renders sound into a buffer.
class MODULES_EXPORT AudioContext : public BaseAudioContext,
                                    public mojom::blink::PermissionObserver,
                                    public mojom::blink::MediaDevicesListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioContext* Create(ExecutionContext*,
                              const AudioContextOptions*,
                              ExceptionState&);

  AudioContext(LocalDOMWindow&,
               const WebAudioLatencyHint&,
               std::optional<float> sample_rate,
               WebAudioSinkDescriptor sink_descriptor,
               bool update_echo_cancellation_on_first_start);
  ~AudioContext() override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(sinkchange, kSinkchange)

  void Trace(Visitor*) const override;

  // For ContextLifeCycleObserver
  void ContextDestroyed() final;
  bool HasPendingActivity() const override;

  // Cannot be called from the audio thread.
  RealtimeAudioDestinationNode* GetRealtimeAudioDestinationNode() const;

  ScriptPromise<IDLUndefined> closeContext(ScriptState*, ExceptionState&);
  bool IsContextCleared() const final;

  ScriptPromise<IDLUndefined> suspendContext(ScriptState*, ExceptionState&);
  ScriptPromise<IDLUndefined> resumeContext(ScriptState*, ExceptionState&);

  bool HasRealtimeConstraint() final { return true; }

  bool IsPullingAudioGraph() const final;

  AudioTimestamp* getOutputTimestamp(ScriptState*) const;
  double baseLatency() const;
  double outputLatency() const;
  AudioPlayoutStats* playoutStats();

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

  bool HandlePreRenderTasks(uint32_t frames_to_process,
                            const AudioIOPosition* output_position,
                            const AudioCallbackMetric* metric,
                            base::TimeDelta playout_delay,
                            const media::AudioGlitchInfo& glitch_info) final;

  // Called at the end of each render quantum.
  void HandlePostRenderTasks() final;

  void HandleAudibility(AudioBus* destination_bus);

  AudioCallbackMetric GetCallbackMetric() const;

  // Returns the audio buffer duration of the output driving playout of
  // AudioDestination.
  base::TimeDelta PlatformBufferDuration() const;

  // mojom::blink::PermissionObserver
  void OnPermissionStatusChange(mojom::blink::PermissionStatus) override;

  Member<V8UnionAudioSinkInfoOrString> sinkId() const { return v8_sink_id_; }

  WebAudioSinkDescriptor GetSinkDescriptor() const { return sink_descriptor_; }

  ScriptPromise<IDLUndefined> setSinkId(ScriptState*,
                                        const V8UnionAudioSinkOptionsOrString*,
                                        ExceptionState&);

  void NotifySetSinkIdBegins();
  void NotifySetSinkIdIsDone(WebAudioSinkDescriptor);

  HeapDeque<Member<SetSinkIdResolver>>& GetSetSinkIdResolver() {
    return set_sink_id_resolvers_;
  }

  // mojom::blink::MediaDevicesListener
  void OnDevicesChanged(mojom::blink::MediaDeviceType,
                        const Vector<WebMediaDeviceInfo>&) override;

  // A helper function to validate the given sink descriptor. See:
  // webaudio.github.io/web-audio-api/#validating-sink-identifier
  bool IsValidSinkDescriptor(const WebAudioSinkDescriptor&);

  void OnRenderError();

  // Methods for unit tests
  void set_was_audible_for_testing(bool value) { was_audible_ = value; }
  void invoke_onrendererror_from_platform_for_testing();

 protected:
  void Uninitialize() final;

 private:
  friend class AudioPlayoutStats;  // For TransferAudioFrameStatsTo().
  friend class AudioContextAutoplayTest;
  friend class AudioContextTest;
  FRIEND_TEST_ALL_PREFIXES(AudioContextTest, MediaDevicesService);
  FRIEND_TEST_ALL_PREFIXES(AudioContextTest,
                           OnRenderErrorFromPlatformDestination);

  // Corresponds to
  // https://wicg.github.io/web_audio_playout/#audioplayoutstats-interface.
  class AudioFrameStats {
   public:
    AudioFrameStats() = default;
    AudioFrameStats(const AudioFrameStats&) = delete;
    AudioFrameStats& operator=(const AudioFrameStats&) = delete;
    ~AudioFrameStats() = default;

    // Updates the stats with information from a new buffer.
    void Update(size_t playout_frames,
                int sample_rate,
                base::TimeDelta playout_latency,
                const media::AudioGlitchInfo& glitch_info) {
      accumulator_.Update(playout_frames, sample_rate, playout_latency,
                          glitch_info);
    }

    // Absorbs stats from an object that contains stats from a more recent
    // interval. This merges the latency statistics into this object, and resets
    // them on the |from| object. |from|'s latency information interval should
    // start where |this|'s latency information interval ends. The frame
    // counters, frame durations, and current latency are simply copied from
    // |from|.
    void Absorb(AudioFrameStats& from) {
      accumulator_.Absorb(from.accumulator_);
    }

    base::TimeDelta FallbackFramesDuration() {
      return accumulator_.glitch_frames_duration();
    }

    size_t FallbackFramesEvents() { return accumulator_.glitch_event_count(); }

    base::TimeDelta TotalFramesDuration() {
      return accumulator_.glitch_frames_duration() +
             accumulator_.observed_frames_duration();
    }

    base::TimeDelta AverageLatency() { return accumulator_.average_latency(); }

    base::TimeDelta MinimumLatency() { return accumulator_.min_latency(); }

    base::TimeDelta MaximumLatency() { return accumulator_.max_latency(); }

   private:
    AudioFrameStatsAccumulator accumulator_;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AutoplayStatus {
    // The AudioContext failed to activate because of user gesture requirements.
    kFailed = 0,
    // Same as AutoplayStatusFailed but start() on a node was called with a user
    // gesture.
    // This value is no longer used but the enum entry should not be re-used
    // because it is used for metrics.
    // kAutoplayStatusFailedWithStart = 1,
    // The AudioContext had user gesture requirements and was able to activate
    // with a user gesture.
    kSucceeded = 2,

    kMaxValue = kSucceeded,
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
    kMaxValue = kSourceNodeStart,
  };

  // If possible, allows autoplay for the AudioContext and marke it as allowed
  // by the given type.
  void MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType);

  // Returns whether the AudioContext is allowed to start rendering.
  bool IsAllowedToStart() const;

  // Record the current autoplay metrics.
  void RecordAutoplayMetrics();

  // Starts rendering via AudioDestinationNode. This sets the self-referencing
  // pointer to this object.
  void StartRendering() override;

  // Called when the context is being closed to stop rendering audio and clean
  // up handlers. This clears the self-referencing pointer, making this object
  // available for the potential GC.
  void StopRendering();

  // Called when suspending the context to stop reundering audio, but don't
  // clean up handlers because we expect to be resuming where we left off.
  void SuspendRendering();

  void DidClose();

  // Called by the audio thread to handle Promises for resume() and suspend(),
  // posting a main thread task to perform the actual resolving, if needed.
  void ResolvePromisesForUnpause();

  AudioIOPosition OutputPosition() const;

  // Send notification to browser that an AudioContext has started or stopped
  // playing audible audio.
  void NotifyAudibleAudioStarted();
  void NotifyAudibleAudioStopped();

  void EnsureAudioContextManagerService();
  void OnAudioContextManagerServiceConnectionError();

  void SendLogMessage(const String& message);

  void DidInitialPermissionCheck(mojom::blink::PermissionDescriptorPtr,
                                 mojom::blink::PermissionStatus);
  double GetOutputLatencyQuantizingFactor() const;

  void InitializeMediaDeviceService();
  void UninitializeMediaDeviceService();

  // Callback from blink::mojom::MediaDevicesDispatcherHost::EnumerateDevices().
  void DevicesEnumerated(const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
                         Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
                             video_input_capabilities,
                         Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
                             audio_input_capabilities);

  // A helper function used to update `v8_sink_id_` whenever `sink_id_` is
  // updated.
  void UpdateV8SinkId();

  // Called on prerendering activation time if this AudioContext is blocked by
  // prerendering.
  void ResumeOnPrerenderActivation();

  // Passes `audio_frame_stats_` to be absorbed by `receiver`.
  void TransferAudioFrameStatsTo(AudioFrameStats& receiver);

  void HandleRenderError();

  unsigned context_id_;
  Member<ScriptPromiseResolver<IDLUndefined>> close_resolver_;

  AudioIOPosition output_position_;
  AudioCallbackMetric callback_metric_;

  // Accessed only on the thread pulling audio from the graph.
  AudioFrameStats pending_audio_frame_stats_;

  // Protected by the graph lock.
  AudioFrameStats audio_frame_stats_;

  Member<AudioPlayoutStats> audio_playout_stats_;

  // Whether a user gesture is required to start this AudioContext.
  bool user_gesture_required_ = false;

  // Whether this AudioContext is blocked to start because the page is still in
  // prerendering state.
  bool blocked_by_prerendering_ = false;

  // Autoplay status associated with this AudioContext, if any.
  // Will only be set if there is an autoplay policy in place.
  // Will never be set for OfflineAudioContext.
  std::optional<AutoplayStatus> autoplay_status_;

  // Autoplay unlock type for this AudioContext.
  // Will only be set if there is an autoplay policy in place.
  // Will never be set for OfflineAudioContext.
  std::optional<AutoplayUnlockType> autoplay_unlock_type_;

  // Records if start() was ever called for any source node in this context.
  bool source_node_started_ = false;

  // Represents whether a context is suspended by explicit `context.suspend()`.
  bool suspended_by_user_ = false;

  // baseLatency for this context
  double base_latency_ = 0;

  // AudioContextManager for reporting audibility.
  HeapMojoRemote<mojom::blink::AudioContextManager> audio_context_manager_;

  // Keeps track if the output of this destination was audible, before the
  // current rendering quantum.  Used for recording "playback" time.
  bool was_audible_ = false;

  // Counts the number of render quanta where audible sound was played.  We
  // determine audibility on render quantum boundaries, so counting quanta is
  // all that's needed.
  size_t total_audible_renders_ = 0;

  SelfKeepAlive<AudioContext> keep_alive_{this};

  // Initially, we assume that the microphone permission is denied. But this
  // will be corrected after the actual construction.
  mojom::blink::PermissionStatus
      microphone_permission_status_ = mojom::blink::PermissionStatus::DENIED;

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, AudioContext>
      permission_receiver_;

  // Describes the current audio output device.
  WebAudioSinkDescriptor sink_descriptor_;

  // A V8 return value from `AudioContext.sinkId` getter. It gets updated when
  // `sink_descriptor_` above is updated.
  Member<V8UnionAudioSinkInfoOrString> v8_sink_id_;

  // A queue for setSinkId() Promise resolvers. Requests are handled in the
  // order it was received and only one request is handled at a time.
  HeapDeque<Member<SetSinkIdResolver>> set_sink_id_resolvers_;

  // MediaDeviceService for querying device information, and the associated
  // receiver for getting notification.
  HeapMojoRemote<mojom::blink::MediaDevicesDispatcherHost>
      media_device_service_;
  HeapMojoReceiver<mojom::blink::MediaDevicesListener, AudioContext>
      media_device_service_receiver_;

  bool is_media_device_service_initialized_ = false;

  // Stores a list of identifiers for output device.
  HashSet<String> output_device_ids_;

  // `wasRunning` flag for `setSinkId()` state transition. See the
  // implementation of `NotifySetSinkIdBegins()` for details.
  bool sink_transition_flag_was_running_ = false;

  // To keep the record of any render errors reported from the infra during
  // the life cycle of the context.
  bool render_error_occurred_ = false;

  // If a sink ID is given via the constructor or `setSinkId()` method,
  // this is set to `true`.
  bool is_sink_id_given_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_CONTEXT_H_
