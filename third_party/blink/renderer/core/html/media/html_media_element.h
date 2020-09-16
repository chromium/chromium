/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_ELEMENT_H_

#include <memory>

#include "base/optional.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/media/web_audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace cc {
class Layer;
}

namespace blink {

class AudioSourceProviderClient;
class AudioTrack;
class AudioTrackList;
class AutoplayPolicy;
class ContentType;
class CueTimeline;
class Event;
class EventQueue;
class ExceptionState;
class HTMLMediaElementControlsList;
class HTMLSourceElement;
class HTMLTrackElement;
class MediaError;
class MediaSourceAttachment;
class MediaSourceTracer;
class MediaStreamDescriptor;
class ScriptPromiseResolver;
class ScriptState;
class TextTrack;
class TextTrackContainer;
class TextTrackList;
class TimeRanges;
class VideoTrack;
class VideoTrackList;
class WebInbandTextTrack;
class WebRemotePlaybackClient;

class CORE_EXPORT HTMLMediaElement
    : public HTMLElement,
      public Supplementable<HTMLMediaElement>,
      public ActiveScriptWrappable<HTMLMediaElement>,
      public ExecutionContextLifecycleStateObserver,
      private WebMediaPlayerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(HTMLMediaElement, Dispose);

 public:
  // Limits the range of media playback rate.
  static constexpr double kMinPlaybackRate = 0.0625;
  static constexpr double kMaxPlaybackRate = 16.0;

  bool IsMediaElement() const override { return true; }

  static MIMETypeRegistry::SupportsType GetSupportsType(const ContentType&);

  enum class RecordMetricsBehavior { kDoNotRecord, kDoRecord };

  static bool IsHLSURL(const KURL&);

  // If HTMLMediaElement is using MediaTracks (either placeholder or provided
  // by the page).
  static bool MediaTracksEnabledInternally();

  // Notify the HTMLMediaElement that the media controls settings have changed
  // for the given document.
  static void OnMediaControlsEnabledChange(Document*);

  void Trace(Visitor*) const override;

  WebMediaPlayer* GetWebMediaPlayer() const { return web_media_player_.get(); }

  // Returns true if the loaded media has a video track.
  // Note that even an audio element can have video track in cases such as
  // <audio src="video.webm">, in which case this function will return true.
  bool HasVideo() const;
  // Returns true if loaded media has an audio track.
  bool HasAudio() const;

  bool SupportsSave() const;

  bool SupportsLoop() const;

  cc::Layer* CcLayer() const;

  enum DelayedActionType {
    kLoadMediaResource = 1 << 0,
    kLoadTextTrackResource = 1 << 1
  };
  void ScheduleTextTrackResourceLoad();

  bool HasRemoteRoutes() const;

  // error state
  MediaError* error() const;

  // network state
  void SetSrc(const AtomicString&);
  const KURL& currentSrc() const { return current_src_; }

  // Return the URL to be used for downloading the media.
  const KURL& downloadURL() const {
    // If we didn't get a redirected URL from the player, then use the original.
    if (current_src_after_redirects_.IsNull() ||
        current_src_after_redirects_.IsEmpty()) {
      return currentSrc();
    }
    return current_src_after_redirects_;
  }

  void SetSrcObject(MediaStreamDescriptor*);
  MediaStreamDescriptor* GetSrcObject() const { return src_object_.Get(); }

  enum NetworkState {
    kNetworkEmpty,
    kNetworkIdle,
    kNetworkLoading,
    kNetworkNoSource
  };
  NetworkState getNetworkState() const;

  String preload() const;
  void setPreload(const AtomicString&);
  WebMediaPlayer::Preload PreloadType() const;
  String EffectivePreload() const;
  WebMediaPlayer::Preload EffectivePreloadType() const;

  WebTimeRanges BufferedInternal() const;
  TimeRanges* buffered() const;
  void load();
  String canPlayType(ExecutionContext* context, const String& mime_type) const;

  // ready state
  enum ReadyState {
    kHaveNothing,
    kHaveMetadata,
    kHaveCurrentData,
    kHaveFutureData,
    kHaveEnoughData
  };
  ReadyState getReadyState() const;
  bool seeking() const;

  // playback state
  double currentTime() const;
  void setCurrentTime(double);
  double duration() const;
  bool paused() const;
  double defaultPlaybackRate() const;
  void setDefaultPlaybackRate(double);
  double playbackRate() const;
  void setPlaybackRate(double, ExceptionState& = ASSERT_NO_EXCEPTION);
  TimeRanges* played();
  WebTimeRanges SeekableInternal() const;
  TimeRanges* seekable() const;
  bool ended() const;
  bool Autoplay() const;
  bool Loop() const;
  void SetLoop(bool);
  ScriptPromise playForBindings(ScriptState*);
  base::Optional<DOMExceptionCode> Play();
  void pause();
  double latencyHint() const;
  void setLatencyHint(double);
  bool preservesPitch() const;
  void setPreservesPitch(bool);
  void FlingingStarted();
  void FlingingStopped();

  // statistics
  uint64_t webkitAudioDecodedByteCount() const;
  uint64_t webkitVideoDecodedByteCount() const;

  // media source extensions
  void CloseMediaSource();
  void DurationChanged(double duration, bool request_seek);

  // controls
  bool ShouldShowControls(
      const RecordMetricsBehavior = RecordMetricsBehavior::kDoNotRecord) const;
  DOMTokenList* controlsList() const;
  HTMLMediaElementControlsList* ControlsListInternal() const;
  double volume() const;
  void setVolume(double, ExceptionState& = ASSERT_NO_EXCEPTION);
  bool muted() const;
  void setMuted(bool);
  virtual bool SupportsPictureInPicture() const { return false; }

  void TogglePlayState();

  AudioTrackList& audioTracks();
  void AudioTrackChanged(AudioTrack*);

  VideoTrackList& videoTracks();
  void SelectedVideoTrackChanged(VideoTrack*);

  TextTrack* addTextTrack(const AtomicString& kind,
                          const AtomicString& label,
                          const AtomicString& language,
                          ExceptionState&);

  TextTrackList* textTracks();
  CueTimeline& GetCueTimeline();

  // Implements the "forget the media element's media-resource-specific tracks"
  // algorithm in the HTML5 spec.
  void ForgetResourceSpecificTracks();

  void DidAddTrackElement(HTMLTrackElement*);
  void DidRemoveTrackElement(HTMLTrackElement*);

  void HonorUserPreferencesForAutomaticTextTrackSelection();

  bool TextTracksAreReady() const;
  void ConfigureTextTrackDisplay();
  void UpdateTextTrackDisplay();
  double LastSeekTime() const { return last_seek_time_; }
  void TextTrackReadyStateChanged(TextTrack*);

  void TextTrackModeChanged(TextTrack*);
  void DisableAutomaticTextTrackSelection();

  // EventTarget function.
  // Both Node (via HTMLElement) and ExecutionContextLifecycleStateObserver
  // define this method, which causes an ambiguity error at compile time. This
  // class's constructor ensures that both implementations return document, so
  // return the result of one of them here.
  using HTMLElement::GetExecutionContext;

  bool IsFullscreen() const;
  virtual bool UsesOverlayFullscreenVideo() const { return false; }

  bool HasClosedCaptions() const;
  bool TextTracksVisible() const;

  static void SetTextTrackKindUserPreferenceForAllMediaElements(Document*);
  void AutomaticTrackSelectionForUpdatedUserPreference();

  // Returns the MediaControls, or null if they have not been added yet.
  // Note that this can be non-null even if there is no controls attribute.
  MediaControls* GetMediaControls() const;

  // Notifies the media element that the media controls became visible, so
  // that text track layout may be updated to avoid overlapping them.
  void MediaControlsDidBecomeVisible();

  void SourceWasRemoved(HTMLSourceElement*);
  void SourceWasAdded(HTMLSourceElement*);

  // ScriptWrappable functions.
  bool HasPendingActivity() const override;

  AudioSourceProviderClient* AudioSourceNode() { return audio_source_node_; }
  void SetAudioSourceNode(AudioSourceProviderClient*);

  AudioSourceProvider& GetAudioSourceProvider() {
    return audio_source_provider_;
  }

  enum InvalidURLAction { kDoNothing, kComplain };
  bool IsSafeToLoadURL(const KURL&, InvalidURLAction);

  // Checks to see if current media data is CORS-same-origin.
  bool IsMediaDataCorsSameOrigin() const;

  void ScheduleEvent(Event*);

  // Returns the "effective media volume" value as specified in the HTML5 spec.
  double EffectiveMediaVolume() const;

  // Predicates also used when dispatching wrapper creation (cf.
  // [SpecialWrapFor] IDL attribute usage.)
  virtual bool IsHTMLAudioElement() const { return false; }
  virtual bool IsHTMLVideoElement() const { return false; }

  void VideoWillBeDrawnToCanvas() const;

  const WebRemotePlaybackClient* RemotePlaybackClient() const {
    return remote_playback_client_;
  }

  const AutoplayPolicy& GetAutoplayPolicy() const { return *autoplay_policy_; }

  WebMediaPlayer::LoadType GetLoadType() const;

  bool HasMediaSource() const { return media_source_attachment_.get(); }

  // Return true if element is paused and won't resume automatically if it
  // becomes visible again.
  bool PausedWhenVisible() const;

  void SetCcLayerForTesting(cc::Layer* layer) { SetCcLayer(layer); }

  bool IsShowPosterFlagSet() const { return show_poster_flag_; }

 protected:
  // Assert the correct order of the children in shadow dom when DCHECK is on.
  static void AssertShadowRootChildren(ShadowRoot&);

  HTMLMediaElement(const QualifiedName&, Document&);
  ~HTMLMediaElement() override;
  void Dispose();

  void ParseAttribute(const AttributeModificationParams&) override;
  void FinishParsingChildren() final;
  bool IsURLAttribute(const Attribute&) const override;
  void AttachLayoutTree(AttachContext&) override;
  void ParserDidSetAttributes() override;
  void CloneNonAttributePropertiesFrom(const Element&,
                                       CloneChildrenFlag) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  // Return true if media is cross origin from the current document
  // and has not passed a cors check, meaning that we should return
  // as little information as possible about it.
  bool MediaShouldBeOpaque() const;

  void DidMoveToNewDocument(Document& old_document) override;
  virtual KURL PosterImageURL() const { return KURL(); }

  // Called after the creation of |web_media_player_|.
  virtual void OnWebMediaPlayerCreated() {}

  void UpdateLayoutObject();

 private:
  // Friend class for testing.
  friend class ContextMenuControllerTest;
  friend class VideoWakeLockTest;
  friend class PictureInPictureControllerTest;

  bool HasPendingActivityInternal() const;

  void ResetMediaPlayerAndMediaSource();

  bool AlwaysCreateUserAgentShadowRoot() const final { return true; }
  bool AreAuthorShadowsAllowed() const final { return false; }

  bool SupportsFocus() const final;
  bool IsMouseFocusable() const final;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void DidRecalcStyle(const StyleRecalcChange) final;

  bool CanStartSelection() const override { return false; }

  bool IsInteractiveContent() const final;

  // ExecutionContextLifecycleStateObserver functions.
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override;
  void ContextDestroyed() override;

  virtual void OnPlay() {}
  virtual void OnLoadStarted() {}
  virtual void OnLoadFinished() {}

  void SetShowPosterFlag(bool value);

  void SetReadyState(ReadyState);
  void SetNetworkState(WebMediaPlayer::NetworkState);

  // WebMediaPlayerClient implementation.
  void NetworkStateChanged() final;
  void ReadyStateChanged() final;
  void TimeChanged() final;
  void Repaint() final;
  void DurationChanged() final;
  void SizeChanged() final;

  void SetCcLayer(cc::Layer*) final;
  WebMediaPlayer::TrackId AddAudioTrack(const WebString&,
                                        WebMediaPlayerClient::AudioTrackKind,
                                        const WebString&,
                                        const WebString&,
                                        bool) final;
  void RemoveAudioTrack(WebMediaPlayer::TrackId) final;
  WebMediaPlayer::TrackId AddVideoTrack(const WebString&,
                                        WebMediaPlayerClient::VideoTrackKind,
                                        const WebString&,
                                        const WebString&,
                                        bool) final;
  void RemoveVideoTrack(WebMediaPlayer::TrackId) final;
  void AddTextTrack(WebInbandTextTrack*) final;
  void RemoveTextTrack(WebInbandTextTrack*) final;
  void MediaSourceOpened(WebMediaSource*) final;
  void RequestSeek(double) final;
  void RemotePlaybackCompatibilityChanged(const WebURL&,
                                          bool is_compatible) final;
  void OnBecamePersistentVideo(bool) override {}
  bool HasSelectedVideoTrack() final;
  WebMediaPlayer::TrackId GetSelectedVideoTrackId() final;
  bool WasAlwaysMuted() final;
  bool HasNativeControls() final;
  bool IsAudioElement() final;
  WebMediaPlayer::DisplayType DisplayType() const override;
  WebRemotePlaybackClient* RemotePlaybackClient() final {
    return remote_playback_client_;
  }
  std::vector<TextTrackMetadata> GetTextTrackMetadata() override;
  gfx::ColorSpace TargetColorSpace() override;
  bool WasAutoplayInitiated() override;
  bool IsInAutoPIP() const override { return false; }
  void RequestPlay() final;
  void RequestPause() final;
  void RequestMuted(bool muted) final;
  void RequestEnterPictureInPicture() override {}
  void RequestExitPictureInPicture() override {}

  void LoadTimerFired(TimerBase*);
  void ProgressEventTimerFired(TimerBase*);
  void PlaybackProgressTimerFired(TimerBase*);
  void ScheduleTimeupdateEvent(bool periodic_event);
  void StartPlaybackProgressTimer();
  void StartProgressEventTimer();
  void StopPeriodicTimers();

  void Seek(double time);
  void FinishSeek();
  void AddPlayedRange(double start, double end);

  // FIXME: Rename to scheduleNamedEvent for clarity.
  void ScheduleEvent(const AtomicString& event_name);

  // loading
  void InvokeLoadAlgorithm();
  void InvokeResourceSelectionAlgorithm();
  void LoadInternal();
  void SelectMediaResource();
  void LoadResource(const WebMediaPlayerSource&, const String& content_type);
  void StartPlayerLoad();
  void SetPlayerPreload();
  void ScheduleNextSourceChild();
  void LoadSourceFromObject();
  void LoadSourceFromAttribute();
  void LoadNextSourceChild();
  void ClearMediaPlayer();
  void ClearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  bool HavePotentialSourceChild();
  void NoneSupported(const String&);
  void MediaEngineError(MediaError*);
  void CancelPendingEventsAndCallbacks();
  void WaitForSourceChange();
  void SetIgnorePreloadNone();

  KURL SelectNextSourceChild(String* content_type, InvalidURLAction);

  void MediaLoadingFailed(WebMediaPlayer::NetworkState, const String&);

  // deferred loading (preload=none)
  bool LoadIsDeferred() const;
  void DeferLoad();
  void CancelDeferredLoad();
  void StartDeferredLoad();
  void ExecuteDeferredLoad();
  void DeferredLoadTimerFired(TimerBase*);

  void MarkCaptionAndSubtitleTracksAsUnconfigured();

  // This does not check user gesture restrictions.
  void PlayInternal();

  // This does not stop autoplay visibility observation.
  void PauseInternal();

  void UpdatePlayState();
  bool PotentiallyPlaying() const;
  bool StoppedDueToErrors() const;
  bool CouldPlayIfEnoughData() const override;

  // Generally the presence of the loop attribute should be considered to mean
  // playback has not "ended", as "ended" and "looping" are mutually exclusive.
  // See
  // https://html.spec.whatwg.org/C/#ended-playback
  enum class LoopCondition { kIncluded, kIgnored };
  bool EndedPlayback(LoopCondition = LoopCondition::kIncluded) const;

  void SetShouldDelayLoadEvent(bool);

  double EarliestPossiblePosition() const;
  double CurrentPlaybackPosition() const;
  double OfficialPlaybackPosition() const;
  void SetOfficialPlaybackPosition(double) const;
  void RequireOfficialPlaybackPositionUpdate() const;

  void EnsureMediaControls();
  void UpdateControlsVisibility();

  TextTrackContainer& EnsureTextTrackContainer();

  void ChangeNetworkStateFromLoadingToIdle();

  WebMediaPlayer::CorsMode CorsMode() const;

  // Returns the "direction of playback" value as specified in the HTML5 spec.
  enum DirectionOfPlayback { kBackward, kForward };
  DirectionOfPlayback GetDirectionOfPlayback() const;

  // Creates placeholder AudioTrack and/or VideoTrack objects when
  // WebMediaPlayer objects advertise they have audio and/or video, but don't
  // explicitly signal them via addAudioTrack() and addVideoTrack().
  // FIXME: Remove this once all WebMediaPlayer implementations properly report
  // their track info.
  void CreatePlaceholderTracksIfNecessary();

  void SetNetworkState(NetworkState);

  void AudioTracksTimerFired(TimerBase*);

  void ScheduleResolvePlayPromises();
  void ScheduleRejectPlayPromises(DOMExceptionCode);
  void ScheduleNotifyPlaying();
  void ResolveScheduledPlayPromises();
  void RejectScheduledPlayPromises();
  void RejectPlayPromises(DOMExceptionCode, const String&);
  void RejectPlayPromisesInternal(DOMExceptionCode, const String&);

  void OnRemovedFromDocumentTimerFired(TimerBase*);

  void SetError(MediaError* error);
  void ReportCurrentTimeToMediaSource();

  Features GetFeatures() override;

  TaskRunnerTimer<HTMLMediaElement> load_timer_;
  TaskRunnerTimer<HTMLMediaElement> progress_event_timer_;
  TaskRunnerTimer<HTMLMediaElement> playback_progress_timer_;
  TaskRunnerTimer<HTMLMediaElement> audio_tracks_timer_;
  TaskRunnerTimer<HTMLMediaElement> removed_from_document_timer_;

  Member<TimeRanges> played_time_ranges_;
  Member<EventQueue> async_event_queue_;

  double playback_rate_;
  double default_playback_rate_;
  NetworkState network_state_;
  ReadyState ready_state_;
  ReadyState ready_state_maximum_;
  KURL current_src_;
  KURL current_src_after_redirects_;
  Member<MediaStreamDescriptor> src_object_;

  // To prevent potential regression when extended by the MSE API, do not set
  // |error_| outside of constructor and SetError().
  Member<MediaError> error_;

  double volume_;
  double last_seek_time_;

  base::Optional<base::ElapsedTimer> previous_progress_time_;

  // Cached duration to suppress duplicate events if duration unchanged.
  double duration_;

  // The last time a timeupdate event was sent in movie time.
  double last_time_update_event_media_time_;

  // The default playback start position.
  double default_playback_start_position_;

  // Loading state.
  enum LoadState {
    kWaitingForSource,
    kLoadingFromSrcObject,
    kLoadingFromSrcAttr,
    kLoadingFromSourceElement
  };
  LoadState load_state_;
  Member<HTMLSourceElement> current_source_node_;
  Member<Node> next_child_node_to_consider_;

  // "Deferred loading" state (for preload=none).
  enum DeferredLoadState {
    // The load is not deferred.
    kNotDeferred,
    // The load is deferred, and waiting for the task to set the
    // delaying-the-load-event flag (to false).
    kWaitingForStopDelayingLoadEventTask,
    // The load is the deferred, and waiting for a triggering event.
    kWaitingForTrigger,
    // The load is deferred, and waiting for the task to set the
    // delaying-the-load-event flag, after which the load will be executed.
    kExecuteOnStopDelayingLoadEventTask
  };
  DeferredLoadState deferred_load_state_;
  TaskRunnerTimer<HTMLMediaElement> deferred_load_timer_;

  std::unique_ptr<WebMediaPlayer> web_media_player_;
  cc::Layer* cc_layer_;

  // These two fields must be carefully set and reset: the actual derived type
  // of the attachment (same-thread vs cross-thread, for instance) must be the
  // same semantic as the actual derived type of the tracer. Further, if there
  // is no attachment, then there must be no tracer that's tracking an active
  // attachment. Note that some kinds of attachments do not require a tracer;
  // see MediaSourceAttachment::StartAttachingToMediaElement() for details.
  scoped_refptr<MediaSourceAttachment> media_source_attachment_;
  Member<MediaSourceTracer> media_source_tracer_;

  // Stores "official playback position", updated periodically from "current
  // playback position". Official playback position should not change while
  // scripts are running. See setOfficialPlaybackPosition().
  mutable double official_playback_position_;
  mutable bool official_playback_position_needs_update_;

  double fragment_end_time_;

  typedef unsigned PendingActionFlags;
  PendingActionFlags pending_action_flags_;

  // FIXME: HTMLMediaElement has way too many state bits.
  bool playing_ : 1;
  bool should_delay_load_event_ : 1;
  bool have_fired_loaded_data_ : 1;
  bool can_autoplay_ : 1;
  bool muted_ : 1;
  bool paused_ : 1;
  bool seeking_ : 1;
  bool paused_by_context_paused_ : 1;
  bool show_poster_flag_ : 1;

  // data has not been loaded since sending a "stalled" event
  bool sent_stalled_event_ : 1;

  bool ignore_preload_none_ : 1;

  bool text_tracks_visible_ : 1;
  bool should_perform_automatic_track_selection_ : 1;

  bool tracks_are_ready_ : 1;
  bool processing_preference_change_ : 1;

  bool was_always_muted_ : 1;

  // Whether or not |web_media_player_| should apply pitch adjustments at
  // playback raters other than 1.0.
  bool preserves_pitch_ = true;

  Member<AudioTrackList> audio_tracks_;
  Member<VideoTrackList> video_tracks_;
  Member<TextTrackList> text_tracks_;
  HeapVector<Member<TextTrack>> text_tracks_when_resource_selection_began_;

  Member<CueTimeline> cue_timeline_;

  HeapVector<Member<ScriptPromiseResolver>> play_promise_resolvers_;
  TaskHandle play_promise_resolve_task_handle_;
  TaskHandle play_promise_reject_task_handle_;
  HeapVector<Member<ScriptPromiseResolver>> play_promise_resolve_list_;
  HeapVector<Member<ScriptPromiseResolver>> play_promise_reject_list_;
  DOMExceptionCode play_promise_error_code_;

  // HTMLMediaElement and its MediaElementAudioSourceNode in case it is provided
  // die together.
  Member<AudioSourceProviderClient> audio_source_node_;

  // AudioClientImpl wraps an AudioSourceProviderClient.
  // When the audio format is known, Chromium calls setFormat().
  class AudioClientImpl final : public GarbageCollected<AudioClientImpl>,
                                public WebAudioSourceProviderClient {
   public:
    explicit AudioClientImpl(AudioSourceProviderClient* client)
        : client_(client) {}

    ~AudioClientImpl() override = default;

    // WebAudioSourceProviderClient
    void SetFormat(uint32_t number_of_channels, float sample_rate) override;

    void Trace(Visitor*) const;

   private:
    Member<AudioSourceProviderClient> client_;
  };

  // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
  // provideInput() calls into Chromium to get a rendered audio stream.
  class AudioSourceProviderImpl final : public AudioSourceProvider {
    DISALLOW_NEW();

   public:
    AudioSourceProviderImpl() = default;
    ~AudioSourceProviderImpl() override = default;

    // Wraps the given WebAudioSourceProvider.
    void Wrap(scoped_refptr<WebAudioSourceProviderImpl>);

    // AudioSourceProvider
    void SetClient(AudioSourceProviderClient*) override;
    void ProvideInput(AudioBus*, uint32_t frames_to_process) override;

    void Trace(Visitor*) const;

   private:
    scoped_refptr<WebAudioSourceProviderImpl> web_audio_source_provider_;
    Member<AudioClientImpl> client_;
    Mutex provide_input_lock;
  };

  AudioSourceProviderImpl audio_source_provider_;

  friend class AutoplayPolicy;
  friend class AutoplayUmaHelperTest;
  friend class Internals;
  friend class TrackDisplayUpdateScope;
  friend class MediaControlsImplTest;
  friend class HTMLMediaElementTest;
  friend class HTMLMediaElementEventListenersTest;
  friend class HTMLVideoElement;
  friend class MediaControlInputElementTest;
  friend class MediaControlsOrientationLockDelegateTest;
  friend class MediaControlsRotateToFullscreenDelegateTest;
  friend class MediaControlLoadingPanelElementTest;
  friend class ContextMenuControllerTest;
  friend class HTMLVideoElementTest;

  Member<AutoplayPolicy> autoplay_policy_;

  WebRemotePlaybackClient* remote_playback_client_;

  Member<MediaControls> media_controls_;
  Member<HTMLMediaElementControlsList> controls_list_;

  Member<IntersectionObserver> lazy_load_intersection_observer_;
};

template <>
inline bool IsElementOfType<const HTMLMediaElement>(const Node& node) {
  return IsA<HTMLMediaElement>(node);
}
template <>
struct DowncastTraits<HTMLMediaElement> {
  static bool AllowFrom(const Node& node) {
    auto* html_element = DynamicTo<HTMLElement>(node);
    return html_element && AllowFrom(*html_element);
  }
  static bool AllowFrom(const HTMLElement& html_element) {
    return IsA<HTMLAudioElement>(html_element) ||
           IsA<HTMLVideoElement>(html_element);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_ELEMENT_H_
