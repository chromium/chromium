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

#include "third_party/blink/renderer/core/html/media/html_media_element.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "media/base/media_track.h"
#include "services/media_session/public/mojom/media_session.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-shared.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_client.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/fileapi/url_file_api.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_source_element.h"
#include "third_party/blink/renderer/core/html/media/audio_output_device_controller.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_media_element_controls_list.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/html/media/media_error.h"
#include "third_party/blink/renderer/core/html/media/media_fragment_uri_parser.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_handle.h"
#include "third_party/blink/renderer/core/html/media/media_source_tracer.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/automatic_track_selection.h"
#include "third_party/blink/renderer/core/html/track/cue_timeline.h"
#include "third_party/blink/renderer/core/html/track/html_track_element.h"
#include "third_party/blink/renderer/core/html/track/loadable_text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_container.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_media.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/speech/speech_synthesis_base.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_source_provider_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_from_url.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/display/screen_info.h"

#ifndef LOG_MEDIA_EVENTS
// Default to not logging events because so many are generated they can
// overwhelm the rest of the logging.
#define LOG_MEDIA_EVENTS 0
#endif

#ifndef LOG_OFFICIAL_TIME_STATUS
// Default to not logging status of official time because it adds a fair amount
// of overhead and logging.
#define LOG_OFFICIAL_TIME_STATUS 0
#endif

namespace blink {

using WeakMediaElementSet = HeapHashSet<WeakMember<HTMLMediaElement>>;
using DocumentElementSetMap =
    HeapHashMap<WeakMember<Document>, Member<WeakMediaElementSet>>;

namespace {

// When enabled, CSS media queries are supported in <source> elements.
BASE_FEATURE(kVideoSourceMediaQuerySupport,
             "VideoSourceMediaQuerySupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This enum is used to record histograms. Do not reorder.
enum class MediaControlsShow {
  kAttribute = 0,
  kFullscreen,
  kNoScript,
  kNotShown,
  kDisabledSettings,
  kUserExplicitlyEnabled,
  kUserExplicitlyDisabled,
  kMaxValue = kUserExplicitlyDisabled,
};

// The state of the HTMLMediaElement when ProgressEventTimerFired is invoked.
// These values are histogrammed, so please only add values to the end.
enum class ProgressEventTimerState {
  // networkState is not NETWORK_LOADING.
  kNotLoading,
  // MediaShouldBeOpaque() is true.
  kMediaShouldBeOpaque,
  // "progress" event was scheduled.
  kProgress,
  // No progress. The "stalled" event was scheduled.
  kStalled,
  // No progress. No "stalled" event scheduled because a Media Source Attachment
  // is used.
  kHasMediaSourceAttachment,
  // No progress. No "stalled" event scheduled because there was recent
  // progress.
  kRecentProgress,
  // No progress. No "stalled" event scheduled because it was already scheduled.
  kStalledEventAlreadyScheduled,
  kMaxValue = kStalledEventAlreadyScheduled
};

static const base::TimeDelta kStalledNotificationInterval = base::Seconds(3);

String UrlForLoggingMedia(const KURL& url) {
  static const unsigned kMaximumURLLengthForLogging = 128;

  if (url.GetString().length() < kMaximumURLLengthForLogging)
    return url.GetString();
  return url.GetString().GetString().Substring(0, kMaximumURLLengthForLogging) +
         "...";
}

const char* BoolString(bool val) {
  return val ? "true" : "false";
}

DocumentElementSetMap& DocumentToElementSetMap() {
  DEFINE_STATIC_LOCAL(Persistent<DocumentElementSetMap>, map,
                      (MakeGarbageCollected<DocumentElementSetMap>()));
  return *map;
}

void AddElementToDocumentMap(HTMLMediaElement* element, Document* document) {
  DocumentElementSetMap& map = DocumentToElementSetMap();
  WeakMediaElementSet* set = nullptr;
  auto it = map.find(document);
  if (it == map.end()) {
    set = MakeGarbageCollected<WeakMediaElementSet>();
    map.insert(document, set);
  } else {
    set = it->value;
  }
  set->insert(element);
}

void RemoveElementFromDocumentMap(HTMLMediaElement* element,
                                  Document* document) {
  DocumentElementSetMap& map = DocumentToElementSetMap();
  auto it = map.find(document);
  CHECK(it != map.end(), base::NotFatalUntil::M130);
  WeakMediaElementSet* set = it->value;
  set->erase(element);
  if (set->empty())
    map.erase(it);
}

String BuildElementErrorMessage(const String& error) {
  // Prepend a UA-specific-error code before the first ':', to enable better
  // collection and aggregation of UA-specific-error codes from
  // MediaError.message by web apps. WebMediaPlayer::GetErrorMessage() should
  // similarly conform to this format.
  DEFINE_STATIC_LOCAL(const String, element_error_prefix,
                      ("MEDIA_ELEMENT_ERROR: "));
  StringBuilder builder;
  builder.Append(element_error_prefix);
  builder.Append(error);
  return builder.ToString();
}

class AudioSourceProviderClientLockScope {
  STACK_ALLOCATED();

 public:
  explicit AudioSourceProviderClientLockScope(HTMLMediaElement& element)
      : client_(element.AudioSourceNode()) {
    if (client_)
      client_->lock();
  }
  ~AudioSourceProviderClientLockScope() {
    if (client_)
      client_->unlock();
  }

 private:
  AudioSourceProviderClient* client_;
};

bool CanLoadURL(const KURL& url, const String& content_type_str) {
  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

  ContentType content_type(content_type_str);
  String content_mime_type = content_type.GetType().DeprecatedLower();
  String content_type_codecs = content_type.Parameter(codecs);

  // If the MIME type is missing or is not meaningful, try to figure it out from
  // the URL.
  if (content_mime_type.empty() ||
      content_mime_type == "application/octet-stream" ||
      content_mime_type == "text/plain") {
    if (url.ProtocolIsData())
      content_mime_type = MimeTypeFromDataURL(url.GetString());
  }

  // If no MIME type is specified, always attempt to load.
  if (content_mime_type.empty())
    return true;

  // 4.8.12.3 MIME types - In the absence of a specification to the contrary,
  // the MIME type "application/octet-stream" when used with parameters, e.g.
  // "application/octet-stream;codecs=theora", is a type that the user agent
  // knows it cannot render.
  if (content_mime_type != "application/octet-stream" ||
      content_type_codecs.empty()) {
    return MIMETypeRegistry::SupportsMediaMIMEType(content_mime_type,
                                                   content_type_codecs) !=
           MIMETypeRegistry::kNotSupported;
  }

  return false;
}

String PreloadTypeToString(WebMediaPlayer::Preload preload_type) {
  switch (preload_type) {
    case WebMediaPlayer::kPreloadNone:
      return "none";
    case WebMediaPlayer::kPreloadMetaData:
      return "metadata";
    case WebMediaPlayer::kPreloadAuto:
      return "auto";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

bool IsValidPlaybackRate(double rate) {
  return rate == 0.0 || (rate >= HTMLMediaElement::kMinPlaybackRate &&
                         rate <= HTMLMediaElement::kMaxPlaybackRate);
}

std::ostream& operator<<(std::ostream& stream,
                         HTMLMediaElement const& media_element) {
  return stream << static_cast<void const*>(&media_element);
}

}  // anonymous namespace

// static
MIMETypeRegistry::SupportsType HTMLMediaElement::GetSupportsType(
    const ContentType& content_type) {
  // TODO(https://crbug.com/809912): Finding source of mime parsing crash.
  static base::debug::CrashKeyString* content_type_crash_key =
      base::debug::AllocateCrashKeyString("media_content_type",
                                          base::debug::CrashKeySize::Size256);
  base::debug::ScopedCrashKeyString scoped_crash_key(
      content_type_crash_key, content_type.Raw().Utf8().c_str());

  String type = content_type.GetType().DeprecatedLower();
  // The codecs string is not lower-cased because MP4 values are case sensitive
  // per http://tools.ietf.org/html/rfc4281#page-7.
  String type_codecs = content_type.Parameter("codecs");

  if (type.empty())
    return MIMETypeRegistry::kNotSupported;

  // 4.8.12.3 MIME types - The canPlayType(type) method must return the empty
  // string if type is a type that the user agent knows it cannot render or is
  // the type "application/octet-stream"
  if (type == "application/octet-stream")
    return MIMETypeRegistry::kNotSupported;

  // |contentType| could be handled using ParsedContentType, but there are
  // still a lot of sites using codec strings that don't work with the
  // stricter parsing rules.
  MIMETypeRegistry::SupportsType result =
      MIMETypeRegistry::SupportsMediaMIMEType(type, type_codecs);
  return result;
}

bool HTMLMediaElement::IsHLSURL(const KURL& url) {
  // Keep the same logic as in media_codec_util.h.
  if (url.IsNull() || url.IsEmpty())
    return false;

  if (!url.IsLocalFile() && !url.ProtocolIs("http") && !url.ProtocolIs("https"))
    return false;

  return url.GetPath().ToString().EndsWith(".m3u8");
}

// static
void HTMLMediaElement::OnMediaControlsEnabledChange(Document* document) {
  auto it = DocumentToElementSetMap().find(document);
  if (it == DocumentToElementSetMap().end())
    return;
  DCHECK(it->value);
  WeakMediaElementSet& elements = *it->value;
  for (const auto& element : elements) {
    element->UpdateControlsVisibility();
    if (element->GetMediaControls())
      element->GetMediaControls()->OnMediaControlsEnabledChange();
  }
}

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tag_name,
                                   Document& document)
    : HTMLElement(tag_name, document),
      ActiveScriptWrappable<HTMLMediaElement>({}),
      ExecutionContextLifecycleStateObserver(GetExecutionContext()),
      load_timer_(document.GetTaskRunner(TaskType::kInternalMedia),
                  this,
                  &HTMLMediaElement::LoadTimerFired),
      audio_tracks_timer_(document.GetTaskRunner(TaskType::kInternalMedia),
                          this,
                          &HTMLMediaElement::AudioTracksTimerFired),
      removed_from_document_timer_(
          document.GetTaskRunner(TaskType::kInternalMedia),
          this,
          &HTMLMediaElement::OnRemovedFromDocumentTimerFired),
      progress_event_timer_(
          document.GetTaskRunner(TaskType::kInternalMedia),
          WTF::BindRepeating(&HTMLMediaElement::ProgressEventTimerFired,
                             WrapWeakPersistent(this))),
      playback_progress_timer_(
          document.GetTaskRunner(TaskType::kInternalMedia),
          WTF::BindRepeating(&HTMLMediaElement::PlaybackProgressTimerFired,
                             WrapWeakPersistent(this))),
      async_event_queue_(
          MakeGarbageCollected<EventQueue>(GetExecutionContext(),
                                           TaskType::kMediaElementEvent)),
      playback_rate_(1.0f),
      default_playback_rate_(1.0f),
      network_state_(kNetworkEmpty),
      ready_state_(kHaveNothing),
      ready_state_maximum_(kHaveNothing),
      volume_(1.0f),
      last_seek_time_(0),
      duration_(std::numeric_limits<double>::quiet_NaN()),
      last_time_update_event_media_time_(
          std::numeric_limits<double>::quiet_NaN()),
      default_playback_start_position_(0),
      load_state_(kWaitingForSource),
      deferred_load_state_(kNotDeferred),
      deferred_load_timer_(document.GetTaskRunner(TaskType::kInternalMedia),
                           this,
                           &HTMLMediaElement::DeferredLoadTimerFired),
      cc_layer_(nullptr),
      official_playback_position_(0),
      official_playback_position_needs_update_(true),
      fragment_end_time_(std::numeric_limits<double>::quiet_NaN()),
      pending_action_flags_(0),
      playing_(false),
      should_delay_load_event_(false),
      have_fired_loaded_data_(false),
      can_autoplay_(true),
      muted_(false),
      paused_(true),
      seeking_(false),
      paused_by_context_paused_(false),
      show_poster_flag_(true),
      sent_stalled_event_(false),
      ignore_preload_none_(false),
      text_tracks_visible_(false),
      should_perform_automatic_track_selection_(true),
      tracks_are_ready_(true),
      processing_preference_change_(false),
      was_always_muted_(true),
      audio_tracks_(MakeGarbageCollected<AudioTrackList>(*this)),
      video_tracks_(MakeGarbageCollected<VideoTrackList>(*this)),
      audio_source_node_(nullptr),
      speech_synthesis_(nullptr),
      autoplay_policy_(MakeGarbageCollected<AutoplayPolicy>(this)),
      remote_playback_client_(nullptr),
      media_controls_(nullptr),
      controls_list_(MakeGarbageCollected<HTMLMediaElementControlsList>(this)),
      lazy_load_intersection_observer_(nullptr) {
  DVLOG(1) << "HTMLMediaElement(" << *this << ")";

  ResetMojoState();

  LocalFrame* frame = document.GetFrame();
  if (frame) {
    remote_playback_client_ =
        frame->Client()->CreateWebRemotePlaybackClient(*this);
  }

  SetHasCustomStyleCallbacks();
  AddElementToDocumentMap(this, &document);

  UseCounter::Count(document, WebFeature::kHTMLMediaElement);
}

HTMLMediaElement::~HTMLMediaElement() {
  DVLOG(1) << "~HTMLMediaElement(" << *this << ")";
}

void HTMLMediaElement::Dispose() {
  // Destroying the player may cause a resource load to be canceled,
  // which could result in LocalDOMWindow::dispatchWindowLoadEvent() being
  // called via ResourceFetch::didLoadResource(), then
  // FrameLoader::checkCompleted(). But it's guaranteed that the load event
  // doesn't get dispatched during the object destruction.
  // See Document::isDelayingLoadEvent().
  // Also see http://crbug.com/275223 for more details.
  ClearMediaPlayerAndAudioSourceProviderClientWithoutLocking();

  progress_event_timer_.Shutdown();
  playback_progress_timer_.Shutdown();
}

void HTMLMediaElement::DidMoveToNewDocument(Document& old_document) {
  DVLOG(3) << "didMoveToNewDocument(" << *this << ")";

  load_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  progress_event_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  playback_progress_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  audio_tracks_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  deferred_load_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  removed_from_document_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));

  autoplay_policy_->DidMoveToNewDocument(old_document);

  if (cue_timeline_) {
    cue_timeline_->DidMoveToNewDocument(old_document);
  }

  // Stop speaking and set speech_synthesis_ to nullptr so that it is
  // re-created on-demand when SpeechSynthesis() is called.
  if (speech_synthesis_) {
    speech_synthesis_->Cancel();
    speech_synthesis_.Clear();
  }

  if (should_delay_load_event_) {
    GetDocument().IncrementLoadEventDelayCount();
    // Note: Keeping the load event delay count increment on oldDocument that
    // was added when should_delay_load_event_ was set so that destruction of
    // web_media_player_ can not cause load event dispatching in oldDocument.
  } else {
    // Incrementing the load event delay count so that destruction of
    // web_media_player_ can not cause load event dispatching in oldDocument.
    old_document.IncrementLoadEventDelayCount();
  }

  RemoveElementFromDocumentMap(this, &old_document);
  AddElementToDocumentMap(this, &GetDocument());
  SetExecutionContext(GetExecutionContext());

  // FIXME: This is a temporary fix to prevent this object from causing the
  // MediaPlayer to dereference LocalFrame and FrameLoader pointers from the
  // previous document. This restarts the load, as if the src attribute had been
  // set.  A proper fix would provide a mechanism to allow this object to
  // refresh the MediaPlayer's LocalFrame and FrameLoader references on document
  // changes so that playback can be resumed properly.
  // TODO(liberato): Consider checking that the new document's opener is the old
  // document: GetDocument().GetFrame()->Opener() == old_document.GetFrame().
  ignore_preload_none_ = false;

  // Experimental: Try to avoid destroying the media player when transferring a
  // media element to a new document. This is a work in progress, and may cause
  // security and/or stability issues.
  // Normally, moving a player between documents requires destroying the
  // media player because web media player cannot outlive the render frame that
  // holds the element which creates the player. However, when transferring a
  // media player to a same-origin picture-in-picture window opened by this
  // document, it is safe to reuse because a picture-in-picture window is
  // guaranteed not to outlive its opener document because
  // DocumentPictureInPictureController watches the destruction and navigation
  // of the opener's WebContents.
  if (!ShouldReusePlayer(old_document, GetDocument())) {
    // Don't worry about notifications from any previous document if we're not
    // re-using the player.
    if (opener_context_observer_)
      opener_context_observer_->SetContextLifecycleNotifier(nullptr);
    AttachToNewFrame();
  } else if (opener_document_ == GetDocument()) {
    // The element is moving back to the player's opener, so stop worrying.
    DCHECK(opener_context_observer_);
    opener_context_observer_->SetContextLifecycleNotifier(
        opener_document_->GetExecutionContext());
    opener_context_observer_ = nullptr;
    opener_document_ = nullptr;
  } else {
    // Moving to a new document, so make sure that the player's opener is not
    // closed while we're still using it.
    if (!opener_context_observer_) {
      DCHECK(!opener_document_);
      // Only set this when we're going from "original opener" to "elsewhere",
      // in case we're moved from one same-origin window to another.
      //
      // This assumes that the first move is from the opener to the pip window.
      // If `ShouldReusePlayer()` lets the first move be in the other direction,
      // then we'll get this wrong.  Somebody would have to set
      // `opener_document_` correctly before we get here, so we'd end up in the
      // case above, instead.  They'd also have to create the context observer.
      opener_document_ = old_document;
      CHECK(!opener_document_->domWindow()->IsPictureInPictureWindow());
      opener_context_observer_ =
          MakeGarbageCollected<OpenerContextObserver>(this);
    }
    opener_context_observer_->SetContextLifecycleNotifier(
        opener_document_->GetExecutionContext());
  }

  // Decrement the load event delay count on oldDocument now that
  // web_media_player_ has been destroyed and there is no risk of dispatching a
  // load event from within the destructor.
  old_document.DecrementLoadEventDelayCount();

  HTMLElement::DidMoveToNewDocument(old_document);
}

bool HTMLMediaElement::ShouldReusePlayer(Document& old_document,
                                         Document& new_document) const {
  // A NULL frame implies a NULL domWindow, so just check one of them
  if (!old_document.GetFrame() || !new_document.GetFrame()) {
    return false;
  }

  // Don't reuse player if the Document Picture-in-Picture API is disabled for
  // both documents.
  if (!RuntimeEnabledFeatures::DocumentPictureInPictureAPIEnabled(
          old_document.domWindow()->GetExecutionContext()) &&
      !RuntimeEnabledFeatures::DocumentPictureInPictureAPIEnabled(
          new_document.domWindow()->GetExecutionContext())) {
    return false;
  }

  auto* new_origin = new_document.GetFrame()
                         ->LocalFrameRoot()
                         .GetSecurityContext()
                         ->GetSecurityOrigin();
  auto* old_origin = old_document.GetFrame()
                         ->LocalFrameRoot()
                         .GetSecurityContext()
                         ->GetSecurityOrigin();

  if (!old_origin || !new_origin || !old_origin->IsSameOriginWith(new_origin)) {
    return false;
  }

  // If we're moving from the opener to pip window, then the player is already
  // connected to the opener and should stay connected to prevent jank.
  if (new_document.domWindow()->IsPictureInPictureWindow() &&
      new_document.GetFrame()->Opener() == old_document.GetFrame()) {
    return true;
  }

  // If we're moving from the pip window to the opener, then we should only
  // reuse the player if it's already associated with the opener.  In practice,
  // this means that `opener_document_` has been set, since
  // `LocalFrameForOpener()` uses that to decide which frame owns the player.
  //
  // Since we don't currently check if the original document is a pip window in
  // the ctor, that means that creating a video element in the pip window will
  // not be jankless when moved to the opener the first time.  Once it's in the
  // opener (either by being moved there or being created there), moves in both
  // directions will be jankless.
  //
  // It could be made jankless in both directions if we noticed (e.g., in the
  // ctor) that we're being created in a pip document, and set
  // `opener_document_` correctly and create the context observer for it.
  //
  // This logic works whether or not we make the ctor smarter about pip.
  // However, it can be simiplified to skip the `opener_document_` check if
  // we're guaranteed that it's always set properly.
  return (old_document.domWindow()->IsPictureInPictureWindow() &&
          old_document.GetFrame()->Opener() == new_document.GetFrame()) &&
         opener_document_ == &new_document;
}

void HTMLMediaElement::AttachToNewFrame() {
  // The opener has closed, so definitely nothing else should use this.
  opener_document_ = nullptr;
  // Do not ask it to stop notifying us -- if this is a callback from the
  // listener, then it's ExecutionContext has been destroyed and it's not
  // allowed to unregister.
  opener_context_observer_ = nullptr;
  // Reset mojo state that is coupled to |old_document|'s execution context.
  // NOTE: |media_player_host_remote_| is also coupled to |old_document|'s
  // frame.
  ResetMojoState();
  InvokeLoadAlgorithm();
}

void HTMLMediaElement::ResetMojoState() {
  if (media_player_host_remote_)
    media_player_host_remote_->Value().reset();
  media_player_host_remote_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemote<media::mojom::blink::MediaPlayerHost>>>(
      GetExecutionContext());
  if (media_player_observer_remote_set_)
    media_player_observer_remote_set_->Value().Clear();
  media_player_observer_remote_set_ = MakeGarbageCollected<DisallowNewWrapper<
      HeapMojoAssociatedRemoteSet<media::mojom::blink::MediaPlayerObserver>>>(
      GetExecutionContext());
  if (media_player_receiver_set_)
    media_player_receiver_set_->Value().Clear();
  media_player_receiver_set_ =
      MakeGarbageCollected<DisallowNewWrapper<HeapMojoAssociatedReceiverSet<
          media::mojom::blink::MediaPlayer, HTMLMediaElement>>>(
          this, GetExecutionContext());
}

FocusableState HTMLMediaElement::SupportsFocus(
    UpdateBehavior update_behavior) const {
  // TODO(https://crbug.com/911882): Depending on result of discussion, remove.
  if (ownerDocument()->IsMediaDocument()) {
    return FocusableState::kNotFocusable;
  }

  // If no controls specified, we should still be able to focus the element if
  // it has tabIndex.
  if (ShouldShowControls()) {
    return FocusableState::kFocusable;
  }
  return HTMLElement::SupportsFocus(update_behavior);
}

FocusableState HTMLMediaElement::IsFocusableState(
    UpdateBehavior update_behavior) const {
  if (!IsFullscreen()) {
    return SupportsFocus(update_behavior);
  }
  return HTMLElement::IsFocusableState(update_behavior);
}

int HTMLMediaElement::DefaultTabIndex() const {
  return 0;
}

void HTMLMediaElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kSrcAttr) {
    DVLOG(2) << "parseAttribute(" << *this
             << ", kSrcAttr, old=" << params.old_value
             << ", new=" << params.new_value << ")";
    // A change to the src attribute can affect intrinsic size, which in turn
    // requires a style recalc.
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(name));
    // Trigger a reload, as long as the 'src' attribute is present.
    if (!params.new_value.IsNull()) {
      ignore_preload_none_ = false;
      InvokeLoadAlgorithm();
    }
  } else if (name == html_names::kControlsAttr) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementControlsAttribute);
    UpdateControlsVisibility();
  } else if (name == html_names::kControlslistAttr) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementControlsListAttribute);
    if (params.old_value != params.new_value) {
      controls_list_->DidUpdateAttributeValue(params.old_value,
                                              params.new_value);
      if (GetMediaControls())
        GetMediaControls()->OnControlsListUpdated();
    }
  } else if (name == html_names::kPreloadAttr) {
    SetPlayerPreload();
  } else if (name == html_names::kDisableremoteplaybackAttr) {
    // This attribute is an extension described in the Remote Playback API spec.
    // Please see: https://w3c.github.io/remote-playback
    UseCounter::Count(GetDocument(),
                      WebFeature::kDisableRemotePlaybackAttribute);
    if (params.old_value != params.new_value) {
      if (web_media_player_) {
        web_media_player_->RequestRemotePlaybackDisabled(
            !params.new_value.IsNull());
      }
    }
  } else if (name == html_names::kLatencyhintAttr &&
             RuntimeEnabledFeatures::MediaLatencyHintEnabled()) {
    if (web_media_player_)
      web_media_player_->SetLatencyHint(latencyHint());
  } else if (name == html_names::kMutedAttr) {
    if (params.reason == AttributeModificationReason::kByParser) {
      muted_ = true;
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

// This method is being used as a way to know that cloneNode finished cloning
// attribute as there is no callback notifying about the end of a cloning
// operation. Indeed, it is required per spec to set the muted state based on
// the content attribute when the object is created.
void HTMLMediaElement::CloneNonAttributePropertiesFrom(const Element& other,
                                                       NodeCloningData& data) {
  HTMLElement::CloneNonAttributePropertiesFrom(other, data);

  if (FastHasAttribute(html_names::kMutedAttr))
    muted_ = true;
}

void HTMLMediaElement::FinishParsingChildren() {
  HTMLElement::FinishParsingChildren();

  if (Traversal<HTMLTrackElement>::FirstChild(*this))
    ScheduleTextTrackResourceLoad();
}

bool HTMLMediaElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return ShouldShowControls() && HTMLElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLMediaElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutMedia>(this);
}

Node::InsertionNotificationRequest HTMLMediaElement::InsertedInto(
    ContainerNode& insertion_point) {
  DVLOG(3) << "insertedInto(" << *this << ", " << insertion_point << ")";

  HTMLElement::InsertedInto(insertion_point);
  if (insertion_point.isConnected()) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLMediaElementInDocument);
    if ((!FastGetAttribute(html_names::kSrcAttr).empty() ||
         src_object_stream_descriptor_ || src_object_media_source_handle_) &&
        network_state_ == kNetworkEmpty) {
      ignore_preload_none_ = false;
      InvokeLoadAlgorithm();
    }
  }

  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLMediaElement::DidNotifySubtreeInsertionsToDocument() {
  UpdateControlsVisibility();
}

void HTMLMediaElement::RemovedFrom(ContainerNode& insertion_point) {
  DVLOG(3) << "removedFrom(" << *this << ", " << insertion_point << ")";

  removed_from_document_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLMediaElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  UpdateLayoutObject();
}

void HTMLMediaElement::DidRecalcStyle(const StyleRecalcChange change) {
  if (!change.ReattachLayoutTree())
    UpdateLayoutObject();
}

void HTMLMediaElement::ScheduleTextTrackResourceLoad() {
  DVLOG(3) << "scheduleTextTrackResourceLoad(" << *this << ")";

  pending_action_flags_ |= kLoadTextTrackResource;

  if (!load_timer_.IsActive())
    load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void HTMLMediaElement::ScheduleNextSourceChild() {
  // Schedule the timer to try the next <source> element WITHOUT resetting state
  // ala invokeLoadAlgorithm.
  pending_action_flags_ |= kLoadMediaResource;
  load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void HTMLMediaElement::ScheduleNamedEvent(const AtomicString& event_name) {
  Event* event = Event::CreateCancelable(event_name);
  event->SetTarget(this);
  ScheduleEvent(event);
}

void HTMLMediaElement::ScheduleEvent(Event* event) {
#if LOG_MEDIA_EVENTS
  DVLOG(3) << "ScheduleEvent(" << (void*)this << ")"
           << " - scheduling '" << event->type() << "'";
#endif
  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

void HTMLMediaElement::LoadTimerFired(TimerBase*) {
  if (pending_action_flags_ & kLoadTextTrackResource)
    HonorUserPreferencesForAutomaticTextTrackSelection();

  if (pending_action_flags_ & kLoadMediaResource) {
    if (load_state_ == kLoadingFromSourceElement)
      LoadNextSourceChild();
    else
      LoadInternal();
  }

  pending_action_flags_ = 0;
}

MediaError* HTMLMediaElement::error() const {
  return error_.Get();
}

void HTMLMediaElement::SetSrc(const AtomicString& url) {
  setAttribute(html_names::kSrcAttr, url);
}

void HTMLMediaElement::SetSrcObjectVariant(
    SrcObjectVariant src_object_variant) {
  DVLOG(1) << __func__ << "(" << *this << ")";
  src_object_stream_descriptor_ = nullptr;
  src_object_media_source_handle_ = nullptr;
  if (auto** desc = absl::get_if<MediaStreamDescriptor*>(&src_object_variant)) {
    src_object_stream_descriptor_ = *desc;
  } else if (auto** handle =
                 absl::get_if<MediaSourceHandle*>(&src_object_variant)) {
    src_object_media_source_handle_ = *handle;
  }

  DVLOG(2) << __func__
           << ": stream_descriptor=" << src_object_stream_descriptor_
           << ", media_source_handle=" << src_object_media_source_handle_;

  InvokeLoadAlgorithm();
}

HTMLMediaElement::SrcObjectVariant HTMLMediaElement::GetSrcObjectVariant()
    const {
  DVLOG(1) << __func__ << "(" << *this << ")"
           << ": stream_descriptor=" << src_object_stream_descriptor_
           << ", media_source_handle=" << src_object_media_source_handle_;

  // At most one is set.
  DCHECK(!(src_object_stream_descriptor_ && src_object_media_source_handle_));

  if (src_object_media_source_handle_)
    return SrcObjectVariant(src_object_media_source_handle_.Get());

  return SrcObjectVariant(src_object_stream_descriptor_.Get());
}

HTMLMediaElement::NetworkState HTMLMediaElement::getNetworkState() const {
  return network_state_;
}

String HTMLMediaElement::canPlayType(const String& mime_type) const {
  MIMETypeRegistry::SupportsType support =
      GetSupportsType(ContentType(mime_type));

  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kHTMLMediaElement_CanPlayType)) {
    blink::IdentifiabilityMetricBuilder(GetDocument().UkmSourceID())
        .Add(
            blink::IdentifiableSurface::FromTypeAndToken(
                blink::IdentifiableSurface::Type::kHTMLMediaElement_CanPlayType,
                IdentifiabilityBenignStringToken(mime_type)),
            static_cast<uint64_t>(support))
        .Record(GetDocument().UkmRecorder());
  }
  String can_play;

  // 4.8.12.3
  switch (support) {
    case MIMETypeRegistry::kNotSupported:
      can_play = g_empty_string;
      break;
    case MIMETypeRegistry::kMaybeSupported:
      can_play = "maybe";
      break;
    case MIMETypeRegistry::kSupported:
      can_play = "probably";
      break;
  }

  DVLOG(2) << "canPlayType(" << *this << ", " << mime_type << ") -> "
           << can_play;

  return can_play;
}

void HTMLMediaElement::load() {
  DVLOG(1) << "load(" << *this << ")";

  autoplay_policy_->TryUnlockingUserGesture();

  ignore_preload_none_ = true;
  InvokeLoadAlgorithm();
}

// Implements the "media element load algorithm" as defined by
// https://html.spec.whatwg.org/multipage/media.html#media-element-load-algorithm
// TODO(srirama.m): Currently ignore_preload_none_ is reset before calling
// invokeLoadAlgorithm() in all places except load(). Move it inside here
// once microtask is implemented for "Await a stable state" step
// in resource selection algorithm.
void HTMLMediaElement::InvokeLoadAlgorithm() {
  DVLOG(3) << "invokeLoadAlgorithm(" << *this << ")";

  // Perform the cleanup required for the resource load algorithm to run.
  StopPeriodicTimers();
  load_timer_.Stop();
  CancelDeferredLoad();
  // FIXME: Figure out appropriate place to reset LoadTextTrackResource if
  // necessary and set pending_action_flags_ to 0 here.
  pending_action_flags_ &= ~kLoadMediaResource;
  sent_stalled_event_ = false;
  have_fired_loaded_data_ = false;

  autoplay_policy_->StopAutoplayMutedWhenVisible();

  // 1 - Abort any already-running instance of the resource selection algorithm
  // for this element.
  load_state_ = kWaitingForSource;
  current_source_node_ = nullptr;

  // 2 - Let pending tasks be a list of tasks from the media element's media
  // element task source in one of the task queues.
  //
  // 3 - For each task in the pending tasks that would run resolve pending
  // play promises or project pending play prmoises algorithms, immediately
  // resolve or reject those promises in the order the corresponding tasks
  // were queued.
  //
  // TODO(mlamouri): the promises are first resolved then rejected but the
  // order between resolved/rejected promises isn't respected. This could be
  // improved when the same task is used for both cases.
  //
  // TODO(mlamouri): don't run the callback synchronously if we are not allowed
  // to run scripts. It can happen in some edge cases. https://crbug.com/660382
  if (play_promise_resolve_task_handle_.IsActive() &&
      !ScriptForbiddenScope::IsScriptForbidden()) {
    play_promise_resolve_task_handle_.Cancel();
    ResolveScheduledPlayPromises();
  }
  if (play_promise_reject_task_handle_.IsActive() &&
      !ScriptForbiddenScope::IsScriptForbidden()) {
    play_promise_reject_task_handle_.Cancel();
    RejectScheduledPlayPromises();
  }

  // 4 - Remove each task in pending tasks from its task queue.
  CancelPendingEventsAndCallbacks();

  // 5 - If the media element's networkState is set to NETWORK_LOADING or
  // NETWORK_IDLE, queue a task to fire a simple event named abort at the media
  // element.
  if (network_state_ == kNetworkLoading || network_state_ == kNetworkIdle)
    ScheduleNamedEvent(event_type_names::kAbort);

  ResetMediaPlayerAndMediaSource();

  // 6 - If the media element's networkState is not set to NETWORK_EMPTY, then
  // run these substeps
  if (network_state_ != kNetworkEmpty) {
    // 4.1 - Queue a task to fire a simple event named emptied at the media
    // element.
    ScheduleNamedEvent(event_type_names::kEmptied);

    // 4.2 - If a fetching process is in progress for the media element, the
    // user agent should stop it.
    SetNetworkState(kNetworkEmpty);

    // 4.4 - Forget the media element's media-resource-specific tracks.
    ForgetResourceSpecificTracks();

    // 4.5 - If readyState is not set to kHaveNothing, then set it to that
    // state.
    ready_state_ = kHaveNothing;
    ready_state_maximum_ = kHaveNothing;

    DCHECK(!paused_ || play_promise_resolvers_.empty());

    // 4.6 - If the paused attribute is false, then run these substeps
    if (!paused_) {
      // 4.6.1 - Set the paused attribute to true.
      paused_ = true;

      // 4.6.2 - Take pending play promises and reject pending play promises
      // with the result and an "AbortError" DOMException.
      RejectPlayPromises(DOMExceptionCode::kAbortError,
                         "The play() request was interrupted by a new load "
                         "request. https://goo.gl/LdLk22");
    }

    // 4.7 - If seeking is true, set it to false.
    seeking_ = false;

    // 4.8 - Set the current playback position to 0.
    //       Set the official playback position to 0.
    //       If this changed the official playback position, then queue a task
    //       to fire a simple event named timeupdate at the media element.
    // 4.9 - Set the initial playback position to 0.
    SetOfficialPlaybackPosition(0);
    ScheduleTimeupdateEvent(false);
    GetCueTimeline().OnReadyStateReset();

    // 4.10 - Set the timeline offset to Not-a-Number (NaN).
    // 4.11 - Update the duration attribute to Not-a-Number (NaN).
  } else if (!paused_) {
    // TODO(foolip): There is a proposal to always reset the paused state
    // in the media element load algorithm, to avoid a bogus play() promise
    // rejection: https://github.com/whatwg/html/issues/869
    // This is where that change would have an effect, and it is measured to
    // verify the assumption that it's a very rare situation.
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementLoadNetworkEmptyNotPaused);
  }

  // 7 - Set the playbackRate attribute to the value of the defaultPlaybackRate
  // attribute.
  setPlaybackRate(defaultPlaybackRate());

  // 8 - Set the error attribute to null and the can autoplay flag to true.
  SetError(nullptr);
  can_autoplay_ = true;

  // 9 - Invoke the media element's resource selection algorithm.
  InvokeResourceSelectionAlgorithm();

  // 10 - Note: Playback of any previously playing media resource for this
  // element stops.
}

void HTMLMediaElement::InvokeResourceSelectionAlgorithm() {
  DVLOG(3) << "invokeResourceSelectionAlgorithm(" << *this << ")";
  // The resource selection algorithm
  // 1 - Set the networkState to NETWORK_NO_SOURCE
  SetNetworkState(kNetworkNoSource);

  // 2 - Set the element's show poster flag to true
  SetShowPosterFlag(true);

  played_time_ranges_ = MakeGarbageCollected<TimeRanges>();

  // FIXME: Investigate whether these can be moved into network_state_ !=
  // kNetworkEmpty block above
  // so they are closer to the relevant spec steps.
  last_seek_time_ = 0;
  duration_ = std::numeric_limits<double>::quiet_NaN();

  // 3 - Set the media element's delaying-the-load-event flag to true (this
  // delays the load event)
  SetShouldDelayLoadEvent(true);
  if (GetMediaControls() && isConnected())
    GetMediaControls()->Reset();

  // 4 - Await a stable state, allowing the task that invoked this algorithm to
  // continue
  // TODO(srirama.m): Remove scheduleNextSourceChild() and post a microtask
  // instead.  See http://crbug.com/593289 for more details.
  ScheduleNextSourceChild();
}

void HTMLMediaElement::LoadInternal() {
  // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose
  // mode was not in the disabled state when the element's resource selection
  // algorithm last started".
  text_tracks_when_resource_selection_began_.clear();
  if (text_tracks_) {
    for (unsigned i = 0; i < text_tracks_->length(); ++i) {
      TextTrack* track = text_tracks_->AnonymousIndexedGetter(i);
      if (track->mode() != TextTrackMode::kDisabled)
        text_tracks_when_resource_selection_began_.push_back(track);
    }
  }

  SelectMediaResource();
}

void HTMLMediaElement::SelectMediaResource() {
  DVLOG(3) << "selectMediaResource(" << *this << ")";

  enum Mode { kObject, kAttribute, kChildren, kNothing };
  Mode mode = kNothing;

  // 6 - If the media element has an assigned media provider object, then let
  //     mode be object.
  if (src_object_stream_descriptor_ || src_object_media_source_handle_) {
    mode = kObject;
  } else if (FastHasAttribute(html_names::kSrcAttr)) {
    // Otherwise, if the media element has no assigned media provider object
    // but has a src attribute, then let mode be attribute.
    mode = kAttribute;
  } else if (HTMLSourceElement* element =
                 Traversal<HTMLSourceElement>::FirstChild(*this)) {
    // Otherwise, if the media element does not have an assigned media
    // provider object and does not have a src attribute, but does have a
    // source element child, then let mode be children and let candidate be
    // the first such source element child in tree order.
    mode = kChildren;
    next_child_node_to_consider_ = element;
    current_source_node_ = nullptr;
  } else {
    // Otherwise the media element has no assigned media provider object and
    // has neither a src attribute nor a source element child: set the
    // networkState to kNetworkEmpty, and abort these steps; the synchronous
    // section ends.
    // TODO(mlamouri): Setting the network state to empty implies that there
    // should be no |web_media_player_|. However, if a previous playback ended
    // due to an error, we can get here and still have one. Decide on a plan
    // to deal with this properly. https://crbug.com/789737
    load_state_ = kWaitingForSource;
    SetShouldDelayLoadEvent(false);
    if (!web_media_player_ || (ready_state_ < kHaveFutureData &&
                               ready_state_maximum_ < kHaveFutureData)) {
      SetNetworkState(kNetworkEmpty);
    } else {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLMediaElementEmptyLoadWithFutureData);
    }
    UpdateLayoutObject();

    DVLOG(3) << "selectMediaResource(" << *this << "), nothing to load";
    return;
  }

  // 7 - Set the media element's networkState to NETWORK_LOADING.
  SetNetworkState(kNetworkLoading);

  // 8 - Queue a task to fire a simple event named loadstart at the media
  // element.
  ScheduleNamedEvent(event_type_names::kLoadstart);

  // 9 - Run the appropriate steps...
  switch (mode) {
    case kObject:
      LoadSourceFromObject();
      DVLOG(3) << "selectMediaResource(" << *this
               << ", using 'srcObject' attribute";
      break;
    case kAttribute:
      LoadSourceFromAttribute();
      DVLOG(3) << "selectMediaResource(" << *this
               << "), using 'src' attribute url";
      break;
    case kChildren:
      LoadNextSourceChild();
      DVLOG(3) << "selectMediaResource(" << *this << "), using source element";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void HTMLMediaElement::LoadSourceFromObject() {
  DCHECK(src_object_stream_descriptor_ || src_object_media_source_handle_);
  load_state_ = kLoadingFromSrcObject;

  if (src_object_media_source_handle_) {
    DCHECK(!src_object_stream_descriptor_);

    // Retrieve the internal blob URL from the handle that was created in the
    // context where the referenced MediaSource is owned, for the purposes of
    // using existing security and logging logic for loading media from a
    // MediaSource with a blob URL.
    const String media_source_handle_url_ =
        src_object_media_source_handle_->GetInternalBlobURL();
    DCHECK(!media_source_handle_url_.empty());

    KURL media_url = GetDocument().CompleteURL(media_source_handle_url_);
    if (!IsSafeToLoadURL(media_url, kComplain)) {
      MediaLoadingFailed(
          WebMediaPlayer::kNetworkStateFormatError,
          BuildElementErrorMessage(
              "Media load from MediaSourceHandle rejected by safety check"));
      return;
    }

    // No type is available when loading from a MediaSourceHandle, via
    // srcObject, even with an internal MediaSource blob URL.
    LoadResource(WebMediaPlayerSource(WebURL(media_url)), String());
    return;
  }

  // No type is available when the resource comes from the 'srcObject'
  // attribute.
  LoadResource(
      WebMediaPlayerSource(WebMediaStream(src_object_stream_descriptor_)),
      String());
}

void HTMLMediaElement::LoadSourceFromAttribute() {
  load_state_ = kLoadingFromSrcAttr;
  const AtomicString& src_value = FastGetAttribute(html_names::kSrcAttr);

  // If the src attribute's value is the empty string ... jump down to the
  // failed step below
  if (src_value.empty()) {
    DVLOG(3) << "LoadSourceFromAttribute(" << *this << "), empty 'src'";
    MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError,
                       BuildElementErrorMessage("Empty src attribute"));
    return;
  }

  KURL media_url = GetDocument().CompleteURL(src_value);
  if (!IsSafeToLoadURL(media_url, kComplain)) {
    MediaLoadingFailed(
        WebMediaPlayer::kNetworkStateFormatError,
        BuildElementErrorMessage("Media load rejected by URL safety check"));
    return;
  }

  // No type is available when the url comes from the 'src' attribute so
  // MediaPlayer will have to pick a media engine based on the file extension.
  LoadResource(WebMediaPlayerSource(WebURL(media_url)), String());
}

void HTMLMediaElement::LoadNextSourceChild() {
  String content_type;
  KURL media_url = SelectNextSourceChild(&content_type, kComplain);
  if (!media_url.IsValid()) {
    WaitForSourceChange();
    return;
  }

  // Reset the MediaPlayer and MediaSource if any
  ResetMediaPlayerAndMediaSource();

  load_state_ = kLoadingFromSourceElement;
  LoadResource(WebMediaPlayerSource(WebURL(media_url)), content_type);
}

void HTMLMediaElement::LoadResource(const WebMediaPlayerSource& source,
                                    const String& content_type) {
  DCHECK(IsMainThread());
  KURL url;
  if (source.IsURL()) {
    url = source.GetAsURL();
    DCHECK(IsSafeToLoadURL(url, kComplain));
    DVLOG(3) << "loadResource(" << *this << ", " << UrlForLoggingMedia(url)
             << ", " << content_type << ")";
  }

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame) {
    MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError,
                       BuildElementErrorMessage(
                           "Resource load failure: document has no frame"));
    return;
  }

  // The resource fetch algorithm
  SetNetworkState(kNetworkLoading);

  // Set |current_src_| *before* changing to the cache url, the fact that we are
  // loading from the app cache is an internal detail not exposed through the
  // media element API. If loading from an internal MediaSourceHandle object
  // URL, then do not expose that URL to app, but instead hold it for use later
  // in StartPlayerLoad and elsewhere (for origin, security etc checks normally
  // done on |current_src_|.)
  if (src_object_media_source_handle_) {
    DCHECK(!url.IsEmpty());
    current_src_.SetSource(url,
                           SourceMetadata::SourceVisibility::kInvisibleToApp);
  } else {
    current_src_.SetSource(url,
                           SourceMetadata::SourceVisibility::kVisibleToApp);
  }

  // Default this to empty, so that we use |current_src_| unless the player
  // provides one later.
  current_src_after_redirects_ = KURL();

  if (audio_source_node_)
    audio_source_node_->OnCurrentSrcChanged(current_src_.GetSourceIfVisible());

  // Update remote playback client with the new src and consider it incompatible
  // until proved otherwise.
  RemotePlaybackCompatibilityChanged(current_src_.GetSourceIfVisible(), false);

  DVLOG(3) << "loadResource(" << *this << ") - current src if visible="
           << UrlForLoggingMedia(current_src_.GetSourceIfVisible())
           << ", current src =" << UrlForLoggingMedia(current_src_.GetSource())
           << ", src_object_media_source_handle_="
           << src_object_media_source_handle_
           << ", src_object_stream_descriptor_="
           << src_object_stream_descriptor_;

  StartProgressEventTimer();

  SetPlayerPreload();

  DCHECK(!media_source_attachment_);
  DCHECK(!media_source_tracer_);
  DCHECK(!error_);

  bool attempt_load = true;

  if (src_object_media_source_handle_) {
    media_source_attachment_ =
        src_object_media_source_handle_->TakeAttachment();

    // If the attachment is nullptr, then fail the load.
    if (!media_source_attachment_) {
      attempt_load = false;
    }
  } else {
    media_source_attachment_ =
        MediaSourceAttachment::LookupMediaSource(url.GetString());
  }
  if (media_source_attachment_) {
    bool start_result = false;
    media_source_tracer_ =
        media_source_attachment_->StartAttachingToMediaElement(this,
                                                               &start_result);
    if (start_result) {
      // If the associated feature is enabled, auto-revoke the MediaSource
      // object URL that was used for attachment on successful (start of)
      // attachment. This can help reduce memory bloat later if the app does not
      // revoke the object URL explicitly and the object URL was the only
      // remaining strong reference to an attached HTMLMediaElement+MediaSource
      // cycle of objects that could otherwise be garbage-collectable. Don't
      // auto-revoke the internal, unregistered, object URL used to attach via
      // srcObject with a MediaSourceHandle, though.
      if (base::FeatureList::IsEnabled(
              media::kRevokeMediaSourceObjectURLOnAttach) &&
          !src_object_media_source_handle_) {
        URLFileAPI::revokeObjectURL(GetExecutionContext(), url.GetString());
      }
    } else {
      // Forget our reference to the MediaSourceAttachment, so we leave it alone
      // while processing remainder of load failure.
      media_source_attachment_.reset();
      media_source_tracer_ = nullptr;
      attempt_load = false;
    }
  }

  bool can_load_resource =
      source.IsMediaStream() || CanLoadURL(url, content_type);
  if (attempt_load && can_load_resource) {
    DCHECK(!web_media_player_);

    // Conditionally defer the load if effective preload is 'none'.
    // Skip this optional deferral for MediaStream sources or any blob URL,
    // including MediaSource blob URLs.
    if (!source.IsMediaStream() && !url.ProtocolIs("blob") &&
        EffectivePreloadType() == WebMediaPlayer::kPreloadNone) {
      DVLOG(3) << "loadResource(" << *this
               << ") : Delaying load because preload == 'none'";
      DeferLoad();
    } else {
      StartPlayerLoad();
    }
  } else {
    MediaLoadingFailed(
        WebMediaPlayer::kNetworkStateFormatError,
        BuildElementErrorMessage(attempt_load
                                     ? "Unable to load URL due to content type"
                                     : "Unable to attach MediaSource"));
  }
}

LocalFrame* HTMLMediaElement::LocalFrameForPlayer() {
  return opener_document_ ? opener_document_->GetFrame()
                          : GetDocument().GetFrame();
}

bool HTMLMediaElement::IsValidCommand(HTMLElement& invoker,
                                      CommandEventType command) {
  if (!RuntimeEnabledFeatures::HTMLInvokeActionsV2Enabled()) {
    return HTMLElement::IsValidCommand(invoker, command);
  }

  return HTMLElement::IsValidCommand(invoker, command) ||
         command == CommandEventType::kPlaypause ||
         command == CommandEventType::kPause ||
         command == CommandEventType::kPlay ||
         command == CommandEventType::kToggleMuted;
}

bool HTMLMediaElement::HandleCommandInternal(HTMLElement& invoker,
                                             CommandEventType command) {
  CHECK(IsValidCommand(invoker, command));

  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }

  Document& document = GetDocument();
  LocalFrame* frame = document.GetFrame();

  if (command == CommandEventType::kPlaypause) {
    if (paused_) {
      if (LocalFrame::HasTransientUserActivation(frame)) {
        Play();
        return true;
      } else {
        String message = "Media cannot be played without a user gesture.";
        document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning, message));
        return false;
      }
    } else {
      pause();
      return true;
    }
  } else if (command == CommandEventType::kPause) {
    if (!paused_) {
      pause();
    }
    return true;
  } else if (command == CommandEventType::kPlay) {
    if (paused_) {
      if (LocalFrame::HasTransientUserActivation(frame)) {
        Play();
      } else {
        String message = "Media cannot be played without a user gesture.";
        document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning, message));
        return false;
      }
    }
    return true;
  } else if (command == CommandEventType::kToggleMuted) {
    // No user activation check as `setMuted` already handles the autoplay
    // policy check.
    setMuted(!muted_);
    return true;
  }

  return false;
}

void HTMLMediaElement::StartPlayerLoad() {
  DCHECK(!web_media_player_);

  // OOM interventions may destroy the JavaScript context while still allowing
  // the page to operate without JavaScript. The media element is too
  // complicated to continue running in this state, so fail.
  // See https://crbug.com/1345473 for more information.
  if (!GetExecutionContext() ||
      GetDocument().domWindow()->IsContextDestroyed()) {
    MediaLoadingFailed(
        WebMediaPlayer::kNetworkStateFormatError,
        BuildElementErrorMessage(
            "Player load failure: JavaScript context destroyed"));
    return;
  }

  // Due to Document PiP we may have a different execution context than our
  // opener, so we also must check that the LocalFrame of the opener is valid.
  LocalFrame* frame = LocalFrameForPlayer();
  if (!frame) {
    MediaLoadingFailed(
        WebMediaPlayer::kNetworkStateFormatError,
        BuildElementErrorMessage("Player load failure: document has no frame"));
    return;
  }

  WebMediaPlayerSource source;
  if (src_object_stream_descriptor_) {
    source =
        WebMediaPlayerSource(WebMediaStream(src_object_stream_descriptor_));
  } else if (src_object_media_source_handle_) {
    DCHECK(current_src_.GetSourceIfVisible().IsEmpty());
    const KURL& internal_url = current_src_.GetSource();
    DCHECK(!internal_url.IsEmpty());

    source = WebMediaPlayerSource(WebURL(internal_url));
  } else {
    // Filter out user:pass as those two URL components aren't
    // considered for media resource fetches (including for the CORS
    // use-credentials mode.) That behavior aligns with Gecko, with IE
    // being more restrictive and not allowing fetches to such URLs.
    //
    // Spec reference: http://whatwg.org/c/#concept-media-load-resource
    //
    // FIXME: when the HTML spec switches to specifying resource
    // fetches in terms of Fetch (http://fetch.spec.whatwg.org), and
    // along with that potentially also specifying a setting for its
    // 'authentication flag' to control how user:pass embedded in a
    // media resource URL should be treated, then update the handling
    // here to match.
    KURL request_url = current_src_.GetSourceIfVisible();
    if (!request_url.User().empty())
      request_url.SetUser(String());
    if (!request_url.Pass().empty())
      request_url.SetPass(String());

    KURL kurl(request_url);
    source = WebMediaPlayerSource(WebURL(kurl));
  }

  web_media_player_ =
      frame->Client()->CreateWebMediaPlayer(*this, source, this);

  if (!web_media_player_) {
    MediaLoadingFailed(WebMediaPlayer::kNetworkStateFormatError,
                       BuildElementErrorMessage(
                           "Player load failure: error creating media player"));
    return;
  }

  OnWebMediaPlayerCreated();

  // Setup the communication channels between the renderer and browser processes
  // via the MediaPlayer and MediaPlayerObserver mojo interfaces.
  DCHECK(media_player_receiver_set_->Value().empty());
  mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>
      media_player_remote;
  BindMediaPlayerReceiver(
      media_player_remote.InitWithNewEndpointAndPassReceiver());

  GetMediaPlayerHostRemote().OnMediaPlayerAdded(
      std::move(media_player_remote), AddMediaPlayerObserverAndPassReceiver(),
      web_media_player_->GetDelegateId());

  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
  // Make sure if we create/re-create the WebMediaPlayer that we update our
  // wrapper.
  audio_source_provider_.Wrap(web_media_player_->GetAudioSourceProvider());
  web_media_player_->SetVolume(EffectiveMediaVolume());

  web_media_player_->SetPoster(PosterImageURL());

  const auto preload = EffectivePreloadType();
  web_media_player_->SetPreload(preload);

  web_media_player_->RequestRemotePlaybackDisabled(
      FastHasAttribute(html_names::kDisableremoteplaybackAttr));

  if (RuntimeEnabledFeatures::
          MediaPlaybackWhileNotVisiblePermissionPolicyEnabled()) {
    web_media_player_->SetShouldPauseWhenFrameIsHidden(
        !GetDocument().GetExecutionContext()->IsFeatureEnabled(
            mojom::blink::PermissionsPolicyFeature::
                kMediaPlaybackWhileNotVisible,
            ReportOptions::kDoNotReport));
  }

  bool is_cache_disabled = false;
  probe::IsCacheDisabled(GetDocument().GetExecutionContext(),
                         &is_cache_disabled);
  auto load_timing = web_media_player_->Load(GetLoadType(), source, CorsMode(),
                                             is_cache_disabled);
  if (load_timing == WebMediaPlayer::LoadTiming::kDeferred) {
    // Deferred media loading is not part of the spec, but intuition is that
    // this should not hold up the Window's "load" event (similar to user
    // gesture requirements).
    SetShouldDelayLoadEvent(false);
  }

  if (IsFullscreen())
    web_media_player_->EnteredFullscreen();

  web_media_player_->SetLatencyHint(latencyHint());

  web_media_player_->SetPreservesPitch(preservesPitch());

  OnLoadStarted();
}

void HTMLMediaElement::SetPlayerPreload() {
  if (web_media_player_)
    web_media_player_->SetPreload(EffectivePreloadType());

  if (LoadIsDeferred() &&
      EffectivePreloadType() != WebMediaPlayer::kPreloadNone)
    StartDeferredLoad();
}

bool HTMLMediaElement::LoadIsDeferred() const {
  return deferred_load_state_ != kNotDeferred;
}

void HTMLMediaElement::DeferLoad() {
  // This implements the "optional" step 4 from the resource fetch algorithm
  // "If mode is remote".
  DCHECK(!deferred_load_timer_.IsActive());
  DCHECK_EQ(deferred_load_state_, kNotDeferred);
  // 1. Set the networkState to NETWORK_IDLE.
  // 2. Queue a task to fire a simple event named suspend at the element.
  ChangeNetworkStateFromLoadingToIdle();
  // 3. Queue a task to set the element's delaying-the-load-event
  // flag to false. This stops delaying the load event.
  deferred_load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
  // 4. Wait for the task to be run.
  deferred_load_state_ = kWaitingForStopDelayingLoadEventTask;
  // Continued in executeDeferredLoad().
}

void HTMLMediaElement::CancelDeferredLoad() {
  deferred_load_timer_.Stop();
  deferred_load_state_ = kNotDeferred;
}

void HTMLMediaElement::ExecuteDeferredLoad() {
  DCHECK_GE(deferred_load_state_, kWaitingForTrigger);

  // resource fetch algorithm step 4 - continued from deferLoad().

  // 5. Wait for an implementation-defined event (e.g. the user requesting that
  // the media element begin playback).  This is assumed to be whatever 'event'
  // ended up calling this method.
  CancelDeferredLoad();
  // 6. Set the element's delaying-the-load-event flag back to true (this
  // delays the load event again, in case it hasn't been fired yet).
  SetShouldDelayLoadEvent(true);
  // 7. Set the networkState to NETWORK_LOADING.
  SetNetworkState(kNetworkLoading);

  StartProgressEventTimer();

  StartPlayerLoad();
}

void HTMLMediaElement::StartDeferredLoad() {
  if (deferred_load_state_ == kWaitingForTrigger) {
    ExecuteDeferredLoad();
    return;
  }
  if (deferred_load_state_ == kExecuteOnStopDelayingLoadEventTask)
    return;
  DCHECK_EQ(deferred_load_state_, kWaitingForStopDelayingLoadEventTask);
  deferred_load_state_ = kExecuteOnStopDelayingLoadEventTask;
}

void HTMLMediaElement::DeferredLoadTimerFired(TimerBase*) {
  SetShouldDelayLoadEvent(false);

  if (deferred_load_state_ == kExecuteOnStopDelayingLoadEventTask) {
    ExecuteDeferredLoad();
    return;
  }
  DCHECK_EQ(deferred_load_state_, kWaitingForStopDelayingLoadEventTask);
  deferred_load_state_ = kWaitingForTrigger;
}

WebMediaPlayer::LoadType HTMLMediaElement::GetLoadType() const {
  if (media_source_attachment_)
    return WebMediaPlayer::kLoadTypeMediaSource;  // Either via src or srcObject

  if (src_object_stream_descriptor_)
    return WebMediaPlayer::kLoadTypeMediaStream;

  return WebMediaPlayer::kLoadTypeURL;
}

void HTMLMediaElement::DidAudioOutputSinkChanged(
    const String& hashed_device_id) {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnAudioOutputSinkChanged(hashed_device_id);
}

void HTMLMediaElement::SetMediaPlayerHostForTesting(
    mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayerHost> host) {
  media_player_host_remote_->Value().Bind(
      std::move(host), GetDocument().GetTaskRunner(TaskType::kInternalMedia));
}

bool HTMLMediaElement::TextTracksAreReady() const {
  // 4.8.12.11.1 Text track model
  // ...
  // The text tracks of a media element are ready if all the text tracks whose
  // mode was not in the disabled state when the element's resource selection
  // algorithm last started now have a text track readiness state of loaded or
  // failed to load.
  for (const auto& text_track : text_tracks_when_resource_selection_began_) {
    if (text_track->GetReadinessState() == TextTrack::kLoading ||
        text_track->GetReadinessState() == TextTrack::kNotLoaded)
      return false;
  }

  return true;
}

void HTMLMediaElement::TextTrackReadyStateChanged(TextTrack* track) {
  if (web_media_player_ &&
      text_tracks_when_resource_selection_began_.Contains(track)) {
    if (track->GetReadinessState() != TextTrack::kLoading) {
      SetReadyState(
          static_cast<ReadyState>(web_media_player_->GetReadyState()));
    }
  } else {
    // The track readiness state might have changed as a result of the user
    // clicking the captions button. In this case, a check whether all the
    // resources have failed loading should be done in order to hide the CC
    // button.
    // TODO(mlamouri): when an HTMLTrackElement fails to load, it is not
    // propagated to the TextTrack object in a web exposed fashion. We have to
    // keep relying on a custom glue to the controls while this is taken care
    // of on the web side. See https://crbug.com/669977
    if (GetMediaControls() &&
        track->GetReadinessState() == TextTrack::kFailedToLoad) {
      GetMediaControls()->OnTrackElementFailedToLoad();
    }
  }
}

void HTMLMediaElement::TextTrackModeChanged(TextTrack* track) {
  // Mark this track as "configured" so configureTextTracks won't change the
  // mode again.
  if (IsA<LoadableTextTrack>(track))
    track->SetHasBeenConfigured(true);

  if (track->IsRendered()) {
    GetDocument().GetStyleEngine().AddTextTrack(track);
  } else {
    GetDocument().GetStyleEngine().RemoveTextTrack(track);
  }

  ConfigureTextTrackDisplay();

  DCHECK(textTracks()->Contains(track));
  textTracks()->ScheduleChangeEvent();
}

void HTMLMediaElement::DisableAutomaticTextTrackSelection() {
  should_perform_automatic_track_selection_ = false;
}

bool HTMLMediaElement::IsSafeToLoadURL(const KURL& url,
                                       InvalidURLAction action_if_invalid) {
  if (!url.IsValid()) {
    DVLOG(3) << "isSafeToLoadURL(" << *this << ", " << UrlForLoggingMedia(url)
             << ") -> FALSE because url is invalid";
    return false;
  }

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window || !window->GetSecurityOrigin()->CanDisplay(url)) {
    if (action_if_invalid == kComplain) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError,
          "Not allowed to load local resource: " + url.ElidedString()));
    }
    DVLOG(3) << "isSafeToLoadURL(" << *this << ", " << UrlForLoggingMedia(url)
             << ") -> FALSE rejected by SecurityOrigin";
    return false;
  }

  if (!GetExecutionContext()->GetContentSecurityPolicy()->AllowMediaFromSource(
          url)) {
    DVLOG(3) << "isSafeToLoadURL(" << *this << ", " << UrlForLoggingMedia(url)
             << ") -> rejected by Content Security Policy";
    return false;
  }

  return true;
}

bool HTMLMediaElement::IsMediaDataCorsSameOrigin() const {
  if (!web_media_player_)
    return true;

  const auto network_state = web_media_player_->GetNetworkState();
  if (network_state == WebMediaPlayer::kNetworkStateNetworkError)
    return false;

  return !web_media_player_->WouldTaintOrigin();
}

void HTMLMediaElement::StartProgressEventTimer() {
  if (progress_event_timer_.IsActive())
    return;

  previous_progress_time_ = base::ElapsedTimer();
  // 350ms is not magic, it is in the spec!
  progress_event_timer_.StartRepeating(base::Milliseconds(350));
}

void HTMLMediaElement::WaitForSourceChange() {
  DVLOG(3) << "waitForSourceChange(" << *this << ")";

  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 17 - Waiting: Set the element's networkState attribute to the
  // NETWORK_NO_SOURCE value
  SetNetworkState(kNetworkNoSource);

  // 18 - Set the element's show poster flag to true.
  SetShowPosterFlag(true);

  // 19 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  SetShouldDelayLoadEvent(false);

  UpdateLayoutObject();
}

void HTMLMediaElement::NoneSupported(const String& input_message) {
  DVLOG(3) << "NoneSupported(" << *this << ", message='" << input_message
           << "')";

  StopPeriodicTimers();
  load_state_ = kWaitingForSource;
  current_source_node_ = nullptr;

  String empty_string;
  const String& message = MediaShouldBeOpaque() ? empty_string : input_message;

  // 4.8.12.5
  // The dedicated media source failure steps are the following steps:

  // 1 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_SRC_NOT_SUPPORTED.
  SetError(MakeGarbageCollected<MediaError>(
      MediaError::kMediaErrSrcNotSupported, message));

  // 2 - Forget the media element's media-resource-specific text tracks.
  ForgetResourceSpecificTracks();

  // 3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE
  // value.
  SetNetworkState(kNetworkNoSource);

  // 4 - Set the element's show poster flag to true.
  SetShowPosterFlag(true);

  // 5 - Fire a simple event named error at the media element.
  ScheduleNamedEvent(event_type_names::kError);

  // 6 - Reject pending play promises with NotSupportedError.
  ScheduleRejectPlayPromises(PlayPromiseError::kNotSupported);

  CloseMediaSource();

  // 7 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  SetShouldDelayLoadEvent(false);

  UpdateLayoutObject();
}

void HTMLMediaElement::MediaEngineError(MediaError* err) {
  DCHECK_GE(ready_state_, kHaveMetadata);
  DVLOG(3) << "mediaEngineError(" << *this << ", "
           << static_cast<int>(err->code()) << ")";

  // 1 - The user agent should cancel the fetching process.
  StopPeriodicTimers();
  load_state_ = kWaitingForSource;

  // 2 - Set the error attribute to a new MediaError object whose code attribute
  // is set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
  SetError(err);

  // 3 - Queue a task to fire a simple event named error at the media element.
  ScheduleNamedEvent(event_type_names::kError);

  // 4 - Set the element's networkState attribute to the NETWORK_IDLE value.
  SetNetworkState(kNetworkIdle);

  // 5 - Set the element's delaying-the-load-event flag to false. This stops
  // delaying the load event.
  SetShouldDelayLoadEvent(false);

  // 6 - Abort the overall resource selection algorithm.
  current_source_node_ = nullptr;
}

void HTMLMediaElement::CancelPendingEventsAndCallbacks() {
  DVLOG(3) << "cancelPendingEventsAndCallbacks(" << *this << ")";
  async_event_queue_->CancelAllEvents();

  for (HTMLSourceElement* source =
           Traversal<HTMLSourceElement>::FirstChild(*this);
       source; source = Traversal<HTMLSourceElement>::NextSibling(*source))
    source->CancelPendingErrorEvent();
}

void HTMLMediaElement::NetworkStateChanged() {
  SetNetworkState(web_media_player_->GetNetworkState());
}

void HTMLMediaElement::MediaLoadingFailed(WebMediaPlayer::NetworkState error,
                                          const String& input_message) {
  DVLOG(3) << "MediaLoadingFailed(" << *this << ", " << int{error}
           << ", message='" << input_message << "')";

  bool should_be_opaque = MediaShouldBeOpaque();
  if (should_be_opaque)
    error = WebMediaPlayer::kNetworkStateNetworkError;
  String empty_string;
  const String& message = should_be_opaque ? empty_string : input_message;

  StopPeriodicTimers();

  // If we failed while trying to load a <source> element, the movie was never
  // parsed, and there are more <source> children, schedule the next one
  if (ready_state_ < kHaveMetadata &&
      load_state_ == kLoadingFromSourceElement) {
    // resource selection algorithm
    // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM
    // manipulation task source, to fire a simple event named error at the
    // candidate element.
    if (current_source_node_) {
      current_source_node_->ScheduleErrorEvent();
    } else {
      DVLOG(3) << "mediaLoadingFailed(" << *this
               << ") - error event not sent, <source> was removed";
    }

    // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous
    // section consists of all the remaining steps of this algorithm until the
    // algorithm says the synchronous section has ended.

    // 9.Otherwise.11 - Forget the media element's media-resource-specific
    // tracks.
    ForgetResourceSpecificTracks();

    if (HavePotentialSourceChild()) {
      DVLOG(3) << "mediaLoadingFailed(" << *this
               << ") - scheduling next <source>";
      ScheduleNextSourceChild();
    } else {
      DVLOG(3) << "mediaLoadingFailed(" << *this
               << ") - no more <source> elements, waiting";
      WaitForSourceChange();
    }

    return;
  }

  if (error == WebMediaPlayer::kNetworkStateNetworkError &&
      ready_state_ >= kHaveMetadata) {
    MediaEngineError(MakeGarbageCollected<MediaError>(
        MediaError::kMediaErrNetwork, message));
  } else if (error == WebMediaPlayer::kNetworkStateDecodeError) {
    MediaEngineError(
        MakeGarbageCollected<MediaError>(MediaError::kMediaErrDecode, message));
  } else if ((error == WebMediaPlayer::kNetworkStateFormatError ||
              error == WebMediaPlayer::kNetworkStateNetworkError) &&
             (load_state_ == kLoadingFromSrcAttr ||
              (load_state_ == kLoadingFromSrcObject &&
               src_object_media_source_handle_))) {
    if (message.empty()) {
      // Generate a more meaningful error message to differentiate the two types
      // of MEDIA_SRC_ERR_NOT_SUPPORTED.
      NoneSupported(BuildElementErrorMessage(
          error == WebMediaPlayer::kNetworkStateFormatError ? "Format error"
                                                            : "Network error"));
    } else {
      NoneSupported(message);
    }
  }

  UpdateLayoutObject();
}

void HTMLMediaElement::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DVLOG(3) << "setNetworkState(" << *this << ", " << static_cast<int>(state)
           << ") - current state is " << int{network_state_};

  if (state == WebMediaPlayer::kNetworkStateEmpty) {
    // Just update the cached state and leave, we can't do anything.
    SetNetworkState(kNetworkEmpty);
    return;
  }

  if (state == WebMediaPlayer::kNetworkStateFormatError ||
      state == WebMediaPlayer::kNetworkStateNetworkError ||
      state == WebMediaPlayer::kNetworkStateDecodeError) {
    MediaLoadingFailed(state, web_media_player_->GetErrorMessage());
    return;
  }

  if (state == WebMediaPlayer::kNetworkStateIdle) {
    if (network_state_ > kNetworkIdle) {
      ChangeNetworkStateFromLoadingToIdle();
    } else {
      SetNetworkState(kNetworkIdle);
    }
  }

  if (state == WebMediaPlayer::kNetworkStateLoading) {
    if (network_state_ < kNetworkLoading || network_state_ == kNetworkNoSource)
      StartProgressEventTimer();
    SetNetworkState(kNetworkLoading);
  }

  if (state == WebMediaPlayer::kNetworkStateLoaded) {
    if (network_state_ != kNetworkIdle)
      ChangeNetworkStateFromLoadingToIdle();
  }
}

void HTMLMediaElement::ChangeNetworkStateFromLoadingToIdle() {
  progress_event_timer_.Stop();

  if (!MediaShouldBeOpaque()) {
    // Schedule one last progress event so we guarantee that at least one is
    // fired for files that load very quickly.
    if (web_media_player_ && web_media_player_->DidLoadingProgress())
      ScheduleNamedEvent(event_type_names::kProgress);
    ScheduleNamedEvent(event_type_names::kSuspend);
    SetNetworkState(kNetworkIdle);
  } else {
    // TODO(dalecurtis): Replace c-style casts in follow up patch.
    DVLOG(1) << __func__ << "(" << *this
             << ") - Deferred network state change to idle for opaque media";
  }
}

void HTMLMediaElement::ReadyStateChanged() {
  SetReadyState(static_cast<ReadyState>(web_media_player_->GetReadyState()));
}

void HTMLMediaElement::SetReadyState(ReadyState state) {
  DVLOG(3) << "setReadyState(" << *this << ", " << int{state}
           << ") - current state is " << int{ready_state_};

  // Set "wasPotentiallyPlaying" BEFORE updating ready_state_,
  // potentiallyPlaying() uses it
  bool was_potentially_playing = PotentiallyPlaying();

  ReadyState old_state = ready_state_;
  ReadyState new_state = state;

  bool tracks_are_ready = TextTracksAreReady();

  if (new_state == old_state && tracks_are_ready_ == tracks_are_ready)
    return;

  tracks_are_ready_ = tracks_are_ready;

  if (tracks_are_ready) {
    ready_state_ = new_state;
  } else {
    // If a media file has text tracks the readyState may not progress beyond
    // kHaveFutureData until the text tracks are ready, regardless of the state
    // of the media file.
    if (new_state <= kHaveMetadata)
      ready_state_ = new_state;
    else
      ready_state_ = kHaveCurrentData;
  }

  // If we're transitioning to / past kHaveMetadata, then cache the final URL.
  if (old_state < kHaveMetadata && new_state >= kHaveMetadata &&
      web_media_player_) {
    current_src_after_redirects_ =
        KURL(web_media_player_->GetSrcAfterRedirects());

    // Sometimes WebMediaPlayer may load a URL from an in memory cache, which
    // skips notification of insecure content. Ensure we always notify the
    // MixedContentChecker of what happened, even if the load was skipped.
    if (LocalFrame* frame = GetDocument().GetFrame()) {
      const KURL& current_src_for_check = current_src_.GetSource();
      // We don't care about the return value here. The MixedContentChecker will
      // internally notify for insecure content if it needs to regardless of
      // what the return value ends up being for this call.
      MixedContentChecker::ShouldBlockFetch(
          frame,
          HasVideo() ? mojom::blink::RequestContextType::VIDEO
                     : mojom::blink::RequestContextType::AUDIO,
          network::mojom::blink::IPAddressSpace::kUnknown,
          current_src_for_check,
          // Strictly speaking, this check is an approximation; a request could
          // have have redirected back to its original URL, for example.
          // However, the redirect status is only used to prevent leaking
          // information cross-origin via CSP reports, so comparing URLs is
          // sufficient for that purpose.
          current_src_after_redirects_ == current_src_for_check
              ? ResourceRequest::RedirectStatus::kNoRedirect
              : ResourceRequest::RedirectStatus::kFollowedRedirect,
          current_src_after_redirects_, /* devtools_id= */ String(),
          ReportingDisposition::kReport,
          GetDocument().Loader()->GetContentSecurityNotifier());
    }

    // Prior to kHaveMetadata |network_state_| may be inaccurate to avoid side
    // channel leaks. This be a no-op if nothing has changed.
    NetworkStateChanged();
  }

  if (new_state > ready_state_maximum_)
    ready_state_maximum_ = new_state;

  if (network_state_ == kNetworkEmpty)
    return;

  if (seeking_) {
    // 4.8.12.9, step 9 note: If the media element was potentially playing
    // immediately before it started seeking, but seeking caused its readyState
    // attribute to change to a value lower than kHaveFutureData, then a waiting
    // will be fired at the element.
    if (was_potentially_playing && ready_state_ < kHaveFutureData)
      ScheduleNamedEvent(event_type_names::kWaiting);

    // 4.8.12.9 steps 12-14
    if (ready_state_ >= kHaveCurrentData)
      FinishSeek();
  } else {
    if (was_potentially_playing && ready_state_ < kHaveFutureData) {
      // Force an update to official playback position. Automatic updates from
      // currentPlaybackPosition() will be blocked while ready_state_ remains
      // < kHaveFutureData. This blocking is desired after 'waiting' has been
      // fired, but its good to update it one final time to accurately reflect
      // media time at the moment we ran out of data to play.
      SetOfficialPlaybackPosition(CurrentPlaybackPosition());

      // 4.8.12.8
      ScheduleTimeupdateEvent(false);
      ScheduleNamedEvent(event_type_names::kWaiting);
    }
  }

  // Once enough of the media data has been fetched to determine the duration of
  // the media resource, its dimensions, and other metadata...
  if (ready_state_ >= kHaveMetadata && old_state < kHaveMetadata) {
    CreatePlaceholderTracksIfNecessary();

    MediaFragmentURIParser fragment_parser(current_src_.GetSource());
    fragment_end_time_ = fragment_parser.EndTime();

    // Set the current playback position and the official playback position to
    // the earliest possible position.
    SetOfficialPlaybackPosition(EarliestPossiblePosition());

    duration_ = web_media_player_->Duration();
    ScheduleNamedEvent(event_type_names::kDurationchange);

    if (IsHTMLVideoElement())
      ScheduleNamedEvent(event_type_names::kResize);
    ScheduleNamedEvent(event_type_names::kLoadedmetadata);

    if (RuntimeEnabledFeatures::AudioVideoTracksEnabled()) {
      Vector<String> default_tracks = fragment_parser.DefaultTracks();
      if (!default_tracks.empty()) {
        AudioTrack* default_audio_track = nullptr;
        VideoTrack* default_video_track = nullptr;
        // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#error-uri-general
        // Multiple occurrences of the same dimension: only the last valid
        // occurrence of a dimension (e.g., t=10 in #t=2&t=10) is interpreted,
        // all previous occurrences (valid or invalid) SHOULD be ignored by the
        // UA. The track dimension is an exception to this rule: multiple track
        // dimensions are allowed (e.g., #track=1&track=2 selects both tracks 1
        // and 2).
        // Because we can't actually play multiple tracks of the same type, we
        // fall back to only selecting the one which is declared last.
        for (const String& track_id : default_tracks) {
          if (AudioTrack* maybe_track = audioTracks().getTrackById(track_id)) {
            default_audio_track = maybe_track;
          }
          if (VideoTrack* maybe_track = videoTracks().getTrackById(track_id)) {
            default_video_track = maybe_track;
          }
        }
        if (default_audio_track) {
          default_audio_track->setEnabled(true);
        }
        if (default_video_track) {
          default_video_track->setSelected(true);
        }
      }
    }

    bool jumped = false;
    if (default_playback_start_position_ > 0) {
      Seek(default_playback_start_position_);
      jumped = true;
    }
    default_playback_start_position_ = 0;

    double initial_playback_position = fragment_parser.StartTime();
    if (std::isnan(initial_playback_position))
      initial_playback_position = 0;

    if (!jumped && initial_playback_position > 0) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLMediaElementSeekToFragmentStart);
      Seek(initial_playback_position);
      jumped = true;
    }

    UpdateLayoutObject();
  }

  bool is_potentially_playing = PotentiallyPlaying();
  if (ready_state_ >= kHaveCurrentData && old_state < kHaveCurrentData &&
      !have_fired_loaded_data_) {
    // Force an update to official playback position to catch non-zero start
    // times that were not known at kHaveMetadata, but are known now that the
    // first packets have been demuxed.
    SetOfficialPlaybackPosition(CurrentPlaybackPosition());

    have_fired_loaded_data_ = true;
    ScheduleNamedEvent(event_type_names::kLoadeddata);
    SetShouldDelayLoadEvent(false);

    OnLoadFinished();
  }

  if (ready_state_ == kHaveFutureData && old_state <= kHaveCurrentData &&
      tracks_are_ready) {
    ScheduleNamedEvent(event_type_names::kCanplay);
    if (is_potentially_playing)
      ScheduleNotifyPlaying();
  }

  if (ready_state_ == kHaveEnoughData && old_state < kHaveEnoughData &&
      tracks_are_ready) {
    if (old_state <= kHaveCurrentData) {
      ScheduleNamedEvent(event_type_names::kCanplay);
      if (is_potentially_playing)
        ScheduleNotifyPlaying();
    }

    if (autoplay_policy_->RequestAutoplayByAttribute()) {
      paused_ = false;
      SetShowPosterFlag(false);
      GetCueTimeline().InvokeTimeMarchesOn();
      ScheduleNamedEvent(event_type_names::kPlay);
      ScheduleNotifyPlaying();
      can_autoplay_ = false;
    }

    ScheduleNamedEvent(event_type_names::kCanplaythrough);
  }

  UpdatePlayState();
}

void HTMLMediaElement::SetShowPosterFlag(bool value) {
  DVLOG(3) << "SetShowPosterFlag(" << *this << ", " << value
           << ") - current state is " << show_poster_flag_;

  if (value == show_poster_flag_)
    return;

  show_poster_flag_ = value;
  UpdateLayoutObject();
}

void HTMLMediaElement::UpdateLayoutObject() {
  if (GetLayoutObject())
    GetLayoutObject()->UpdateFromElement();
}

void HTMLMediaElement::ProgressEventTimerFired() {
  // The spec doesn't require to dispatch the "progress" or "stalled" events
  // when the resource fetch mode is "local".
  // https://html.spec.whatwg.org/multipage/media.html#concept-media-load-resource
  // The mode is "local" for these sources:
  //
  // MediaStream: The timer is stopped below to prevent the "progress" event
  // from being dispatched more than once. It is dispatched once to match
  // Safari's behavior, even though that's not required by the spec.
  //
  // MediaSource: The "stalled" event is not dispatched but a conscious decision
  // was made to periodically dispatch the "progress" event to allow updates to
  // buffering UIs. Therefore, the timer is not stopped below.
  // https://groups.google.com/a/chromium.org/g/media-dev/c/Y8ITyIFmUC0/m/avBYOy_UFwAJ
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream) {
    progress_event_timer_.Stop();
  }

  if (network_state_ != kNetworkLoading) {
    return;
  }

  // If this is an cross-origin request, and we haven't discovered whether
  // the media is actually playable yet, don't fire any progress events as
  // those may let the page know information about the resource that it's
  // not supposed to know.
  if (MediaShouldBeOpaque()) {
    return;
  }

  DCHECK(previous_progress_time_);

  if (web_media_player_ && web_media_player_->DidLoadingProgress()) {
    ScheduleNamedEvent(event_type_names::kProgress);
    previous_progress_time_ = base::ElapsedTimer();
    sent_stalled_event_ = false;
    UpdateLayoutObject();
  } else if (!media_source_attachment_ &&
             previous_progress_time_->Elapsed() >
                 kStalledNotificationInterval &&
             !sent_stalled_event_) {
    // Note the !media_source_attachment_ condition above. The 'stalled' event
    // is not fired when using MSE. MSE's resource is considered 'local' (we
    // don't manage the download - the app does), so the HTML5 spec text around
    // 'stalled' does not apply. See discussion in https://crbug.com/517240 We
    // also don't need to take any action wrt delaying-the-load-event.
    // MediaSource disables the delayed load when first attached.
    ScheduleNamedEvent(event_type_names::kStalled);
    sent_stalled_event_ = true;
    SetShouldDelayLoadEvent(false);
  }
}

void HTMLMediaElement::AddPlayedRange(double start, double end) {
  DVLOG(3) << "addPlayedRange(" << *this << ", " << start << ", " << end << ")";
  if (!played_time_ranges_)
    played_time_ranges_ = MakeGarbageCollected<TimeRanges>();
  played_time_ranges_->Add(start, end);
}

bool HTMLMediaElement::SupportsSave() const {
  // Check if download is disabled per settings.
  if (GetDocument().GetSettings() &&
      GetDocument().GetSettings()->GetHideDownloadUI()) {
    return false;
  }

  // Get the URL that we'll use for downloading.
  const KURL url = downloadURL();

  // URLs that lead to nowhere are ignored.
  if (url.IsNull() || url.IsEmpty())
    return false;

  // If we have no source, we can't download.
  if (network_state_ == kNetworkEmpty || network_state_ == kNetworkNoSource)
    return false;

  // It is not useful to offer a save feature on local files.
  if (url.IsLocalFile())
    return false;

  // MediaStream can't be downloaded.
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return false;

  // MediaSource can't be downloaded.
  if (HasMediaSource())
    return false;

  // HLS stream shouldn't have a download button.
  if (IsHLSURL(url))
    return false;

  // Infinite streams don't have a clear end at which to finish the download.
  if (duration() == std::numeric_limits<double>::infinity())
    return false;

  return true;
}

bool HTMLMediaElement::SupportsLoop() const {
  // MediaStream can't be looped.
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return false;

  // Infinite streams don't have a clear end at which to loop.
  if (duration() == std::numeric_limits<double>::infinity())
    return false;

  return true;
}

void HTMLMediaElement::SetIgnorePreloadNone() {
  DVLOG(3) << "setIgnorePreloadNone(" << *this << ")";
  ignore_preload_none_ = true;
  SetPlayerPreload();
}

void HTMLMediaElement::Seek(double time) {
  DVLOG(2) << "seek(" << *this << ", " << time << ")";

  // 1 - Set the media element's show poster flag to false.
  SetShowPosterFlag(false);

  // 2 - If the media element's readyState is HAVE_NOTHING, abort these steps.
  // FIXME: remove web_media_player_ check once we figure out how
  // web_media_player_ is going out of sync with readystate.
  // web_media_player_ is cleared but readystate is not set to HAVE_NOTHING.
  if (!web_media_player_ || ready_state_ == kHaveNothing)
    return;

  // Ignore preload none and start load if necessary.
  SetIgnorePreloadNone();

  // Get the current time before setting seeking_, last_seek_time_ is returned
  // once it is set.
  double now = currentTime();

  // 3 - If the element's seeking IDL attribute is true, then another instance
  // of this algorithm is already running. Abort that other instance of the
  // algorithm without waiting for the step that it is running to complete.
  // Nothing specific to be done here.

  // 4 - Set the seeking IDL attribute to true.
  // The flag will be cleared when the engine tells us the time has actually
  // changed.
  seeking_ = true;

  // 6 - If the new playback position is later than the end of the media
  // resource, then let it be the end of the media resource instead.
  time = std::min(time, duration());

  // 7 - If the new playback position is less than the earliest possible
  // position, let it be that position instead.
  time = std::max(time, EarliestPossiblePosition());

  // Ask the media engine for the time value in the movie's time scale before
  // comparing with current time. This is necessary because if the seek time is
  // not equal to currentTime but the delta is less than the movie's time scale,
  // we will ask the media engine to "seek" to the current movie time, which may
  // be a noop and not generate a timechanged callback. This means seeking_
  // will never be cleared and we will never fire a 'seeked' event.
  double media_time = web_media_player_->MediaTimeForTimeValue(time);
  if (time != media_time) {
    DVLOG(3) << "seek(" << *this << ", " << time
             << ") - media timeline equivalent is " << media_time;
    time = media_time;
  }

  // 8 - If the (possibly now changed) new playback position is not in one of
  // the ranges given in the seekable attribute, then let it be the position in
  // one of the ranges given in the seekable attribute that is the nearest to
  // the new playback position. ... If there are no ranges given in the seekable
  // attribute then set the seeking IDL attribute to false and abort these
  // steps.
  WebTimeRanges seekable_ranges = SeekableInternal();

  if (seekable_ranges.empty()) {
    seeking_ = false;
    return;
  }
  time = seekable_ranges.Nearest(time, now);

  if (playing_ && last_seek_time_ < now)
    AddPlayedRange(last_seek_time_, now);

  last_seek_time_ = time;

  // 10 - Queue a task to fire a simple event named seeking at the element.
  ScheduleNamedEvent(event_type_names::kSeeking);

  // 11 - Set the current playback position to the given new playback position.
  web_media_player_->Seek(time);
  web_media_player_->OnTimeUpdate();

  // 14-17 are handled, if necessary, when the engine signals a readystate
  // change or otherwise satisfies seek completion and signals a time change.
}

void HTMLMediaElement::FinishSeek() {
  DVLOG(3) << "finishSeek(" << *this << ")";

  // 14 - Set the seeking IDL attribute to false.
  seeking_ = false;

  // Force an update to officialPlaybackPosition. Periodic updates generally
  // handle this, but may be skipped paused or waiting for data.
  SetOfficialPlaybackPosition(CurrentPlaybackPosition());

  // 15 - Run the time marches on steps.
  GetCueTimeline().InvokeTimeMarchesOn();

  // 16 - Queue a task to fire a simple event named timeupdate at the element.
  ScheduleTimeupdateEvent(false);

  // 17 - Queue a task to fire a simple event named seeked at the element.
  ScheduleNamedEvent(event_type_names::kSeeked);
}

HTMLMediaElement::ReadyState HTMLMediaElement::getReadyState() const {
  return ready_state_;
}

bool HTMLMediaElement::HasVideo() const {
  return web_media_player_ && web_media_player_->HasVideo();
}

bool HTMLMediaElement::HasAudio() const {
  return web_media_player_ && web_media_player_->HasAudio();
}

bool HTMLMediaElement::IsEncrypted() const {
  return is_encrypted_media_;
}

bool HTMLMediaElement::seeking() const {
  return seeking_;
}

// https://www.w3.org/TR/html51/semantics-embedded-content.html#earliest-possible-position
// The earliest possible position is not explicitly exposed in the API; it
// corresponds to the start time of the first range in the seekable attribute’s
// TimeRanges object, if any, or the current playback position otherwise.
double HTMLMediaElement::EarliestPossiblePosition() const {
  WebTimeRanges seekable_ranges = SeekableInternal();
  if (!seekable_ranges.empty())
    return seekable_ranges.front().start;

  return CurrentPlaybackPosition();
}

double HTMLMediaElement::CurrentPlaybackPosition() const {
  // "Official" playback position won't take updates from "current" playback
  // position until ready_state_ > kHaveMetadata, but other callers (e.g.
  // pauseInternal) may still request currentPlaybackPosition at any time.
  // From spec: "Media elements have a current playback position, which must
  // initially (i.e., in the absence of media data) be zero seconds."
  if (ready_state_ == kHaveNothing)
    return 0;

  if (web_media_player_)
    return web_media_player_->CurrentTime();

  if (ready_state_ >= kHaveMetadata) {
    DVLOG(3) << __func__ << " readyState = " << ready_state_
             << " but no webMediaPlayer to provide currentPlaybackPosition";
  }

  return 0;
}

double HTMLMediaElement::OfficialPlaybackPosition() const {
  // Hold updates to official playback position while paused or waiting for more
  // data. The underlying media player may continue to make small advances in
  // currentTime (e.g. as samples in the last rendered audio buffer are played
  // played out), but advancing currentTime while paused/waiting sends a mixed
  // signal about the state of playback.
  bool waiting_for_data = ready_state_ <= kHaveCurrentData;
  if (official_playback_position_needs_update_ && !paused_ &&
      !waiting_for_data) {
    SetOfficialPlaybackPosition(CurrentPlaybackPosition());
  }

#if LOG_OFFICIAL_TIME_STATUS
  static const double kMinCachedDeltaForWarning = 0.01;
  double delta =
      std::abs(official_playback_position_ - CurrentPlaybackPosition());
  if (delta > kMinCachedDeltaForWarning) {
    DVLOG(3) << "CurrentTime(" << (void*)this << ") - WARNING, cached time is "
             << delta << "seconds off of media time when paused/waiting";
  }
#endif

  return official_playback_position_;
}

void HTMLMediaElement::SetOfficialPlaybackPosition(double position) const {
#if LOG_OFFICIAL_TIME_STATUS
  DVLOG(3) << "SetOfficialPlaybackPosition(" << (void*)this
           << ") was:" << official_playback_position_ << " now:" << position;
#endif

  // Internal player position may advance slightly beyond duration because
  // many files use imprecise duration. Clamp official position to duration when
  // known. Duration may be unknown when readyState < HAVE_METADATA.
  official_playback_position_ =
      std::isnan(duration()) ? position : std::min(duration(), position);

  if (official_playback_position_ != position) {
    DVLOG(3) << "setOfficialPlaybackPosition(" << *this
             << ") position:" << position
             << " truncated to duration:" << official_playback_position_;
  }

  // Once set, official playback position should hold steady until the next
  // stable state. We approximate this by using a microtask to mark the
  // need for an update after the current (micro)task has completed. When
  // needed, the update is applied in the next call to
  // officialPlaybackPosition().
  official_playback_position_needs_update_ = false;
  GetDocument().GetAgent().event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&HTMLMediaElement::RequireOfficialPlaybackPositionUpdate,
                    WrapWeakPersistent(this)));
}

void HTMLMediaElement::RequireOfficialPlaybackPositionUpdate() const {
  official_playback_position_needs_update_ = true;
}

double HTMLMediaElement::currentTime() const {
  if (default_playback_start_position_)
    return default_playback_start_position_;

  if (seeking_) {
    DVLOG(3) << "currentTime(" << *this << ") - seeking, returning "
             << last_seek_time_;
    return last_seek_time_;
  }

  return OfficialPlaybackPosition();
}

void HTMLMediaElement::setCurrentTime(double time) {
  // If the media element's readyState is kHaveNothing, then set the default
  // playback start position to that time.
  if (ready_state_ == kHaveNothing) {
    default_playback_start_position_ = time;
  } else {
    Seek(time);
  }

  ReportCurrentTimeToMediaSource();
}

double HTMLMediaElement::duration() const {
  return duration_;
}

bool HTMLMediaElement::paused() const {
  return paused_;
}

double HTMLMediaElement::defaultPlaybackRate() const {
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return 1.0;
  return default_playback_rate_;
}

void HTMLMediaElement::setDefaultPlaybackRate(double rate) {
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return;

  if (default_playback_rate_ == rate || !IsValidPlaybackRate(rate))
    return;

  default_playback_rate_ = rate;
  ScheduleNamedEvent(event_type_names::kRatechange);
}

double HTMLMediaElement::playbackRate() const {
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return 1.0;
  return playback_rate_;
}

void HTMLMediaElement::setPlaybackRate(double rate,
                                       ExceptionState& exception_state) {
  DVLOG(3) << "setPlaybackRate(" << *this << ", " << rate << ")";
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return;

  if (!IsValidPlaybackRate(rate)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementMediaPlaybackRateOutOfRange);

    // When the proposed playbackRate is unsupported, throw a NotSupportedError
    // DOMException and don't update the value.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The provided playback rate (" + String::Number(rate) +
            ") is not in the " + "supported playback range.");

    // Do not update |playback_rate_|.
    return;
  }

  if (playback_rate_ != rate) {
    playback_rate_ = rate;
    ScheduleNamedEvent(event_type_names::kRatechange);
  }

  // FIXME: remove web_media_player_ check once we figure out how
  // web_media_player_ is going out of sync with readystate.
  // web_media_player_ is cleared but readystate is not set to kHaveNothing.
  if (web_media_player_) {
    if (PotentiallyPlaying())
      web_media_player_->SetRate(playbackRate());

    web_media_player_->OnTimeUpdate();
  }

  if (cue_timeline_ && PotentiallyPlaying())
    cue_timeline_->OnPlaybackRateUpdated();
}

HTMLMediaElement::DirectionOfPlayback HTMLMediaElement::GetDirectionOfPlayback()
    const {
  return playback_rate_ >= 0 ? kForward : kBackward;
}

bool HTMLMediaElement::ended() const {
  // 4.8.12.8 Playing the media resource
  // The ended attribute must return true if the media element has ended
  // playback and the direction of playback is forwards, and false otherwise.
  return EndedPlayback() && GetDirectionOfPlayback() == kForward;
}

bool HTMLMediaElement::Autoplay() const {
  return FastHasAttribute(html_names::kAutoplayAttr);
}

String HTMLMediaElement::preload() const {
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return PreloadTypeToString(WebMediaPlayer::kPreloadNone);
  return PreloadTypeToString(PreloadType());
}

void HTMLMediaElement::setPreload(const AtomicString& preload) {
  DVLOG(2) << "setPreload(" << *this << ", " << preload << ")";
  if (GetLoadType() == WebMediaPlayer::kLoadTypeMediaStream)
    return;
  setAttribute(html_names::kPreloadAttr, preload);
}

WebMediaPlayer::Preload HTMLMediaElement::PreloadType() const {
  const AtomicString& preload = FastGetAttribute(html_names::kPreloadAttr);
  if (EqualIgnoringASCIICase(preload, "none")) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLMediaElementPreloadNone);
    return WebMediaPlayer::kPreloadNone;
  }

  if (EqualIgnoringASCIICase(preload, "metadata")) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementPreloadMetadata);
    return WebMediaPlayer::kPreloadMetaData;
  }

  // Force preload to 'metadata' on cellular connections.
  if (GetNetworkStateNotifier().IsCellularConnectionType()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLMediaElementPreloadForcedMetadata);
    return WebMediaPlayer::kPreloadMetaData;
  }

  // Per HTML spec, "The empty string ... maps to the Automatic state."
  // https://html.spec.whatwg.org/C/#attr-media-preload
  if (EqualIgnoringASCIICase(preload, "auto") ||
      EqualIgnoringASCIICase(preload, "")) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLMediaElementPreloadAuto);
    return WebMediaPlayer::kPreloadAuto;
  }

  // "The attribute's missing value default is user-agent defined, though the
  // Metadata state is suggested as a compromise between reducing server load
  // and providing an optimal user experience."

  // The spec does not define an invalid value default:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28950
  UseCounter::Count(GetDocument(), WebFeature::kHTMLMediaElementPreloadDefault);
  return WebMediaPlayer::kPreloadMetaData;
}

String HTMLMediaElement::EffectivePreload() const {
  return PreloadTypeToString(EffectivePreloadType());
}

WebMediaPlayer::Preload HTMLMediaElement::EffectivePreloadType() const {
  if (Autoplay() && !autoplay_policy_->IsGestureNeededForPlayback())
    return WebMediaPlayer::kPreloadAuto;

  WebMediaPlayer::Preload preload = PreloadType();
  if (ignore_preload_none_ && preload == WebMediaPlayer::kPreloadNone)
    return WebMediaPlayer::kPreloadMetaData;

  return preload;
}

ScriptPromise<IDLUndefined> HTMLMediaElement::playForBindings(
    ScriptState* script_state) {
  // We have to share the same logic for internal and external callers. The
  // internal callers do not want to receive a Promise back but when ::play()
  // is called, |play_promise_resolvers_| needs to be populated. What this code
  // does is to populate |play_promise_resolvers_| before calling ::play() and
  // remove the Promise if ::play() failed.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  play_promise_resolvers_.push_back(resolver);

  std::optional<DOMExceptionCode> code = Play();
  if (code) {
    DCHECK(!play_promise_resolvers_.empty());
    play_promise_resolvers_.pop_back();

    String message;
    switch (code.value()) {
      case DOMExceptionCode::kNotAllowedError:
        message = autoplay_policy_->GetPlayErrorMessage();
        break;
      case DOMExceptionCode::kNotSupportedError:
        message = "The element has no supported sources.";
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    resolver->Reject(MakeGarbageCollected<DOMException>(code.value(), message));
    return promise;
  }

  return promise;
}

std::optional<DOMExceptionCode> HTMLMediaElement::Play() {
  DVLOG(2) << "play(" << *this << ")";

  std::optional<DOMExceptionCode> exception_code =
      autoplay_policy_->RequestPlay();

  if (exception_code == DOMExceptionCode::kNotAllowedError) {
    // If we're already playing, then this play would do nothing anyway.
    // Call playInternal to handle scheduling the promise resolution.
    if (!paused_) {
      PlayInternal();
      return std::nullopt;
    }
    return exception_code;
  }

  autoplay_policy_->StopAutoplayMutedWhenVisible();

  if (error_ && error_->code() == MediaError::kMediaErrSrcNotSupported)
    return DOMExceptionCode::kNotSupportedError;

  DCHECK(!exception_code.has_value());

  PlayInternal();

  return std::nullopt;
}

void HTMLMediaElement::PlayInternal() {
  DVLOG(3) << "playInternal(" << *this << ")";

  if (web_media_player_) {
    web_media_player_->SetWasPlayedWithUserActivationAndHighMediaEngagement(
        LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()) &&
        AutoplayPolicy::DocumentHasHighMediaEngagement(GetDocument()));
  }

  // Playback aborts any lazy loading.
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->disconnect();
    lazy_load_intersection_observer_ = nullptr;
  }

  // 4.8.12.8. Playing the media resource
  if (network_state_ == kNetworkEmpty)
    InvokeResourceSelectionAlgorithm();

  // Generally "ended" and "looping" are exclusive. Here, the loop attribute
  // is ignored to seek back to start in case loop was set after playback
  // ended. See http://crbug.com/364442
  if (EndedPlayback(LoopCondition::kIgnored))
    Seek(0);

  if (paused_) {
    paused_ = false;
    SetShowPosterFlag(false);
    GetCueTimeline().InvokeTimeMarchesOn();
    ScheduleNamedEvent(event_type_names::kPlay);

    if (ready_state_ <= kHaveCurrentData)
      ScheduleNamedEvent(event_type_names::kWaiting);
    else if (ready_state_ >= kHaveFutureData)
      ScheduleNotifyPlaying();
  } else if (ready_state_ >= kHaveFutureData) {
    ScheduleResolvePlayPromises();
  }

  can_autoplay_ = false;

  OnPlay();

  SetIgnorePreloadNone();
  UpdatePlayState();
}

void HTMLMediaElement::pause() {
  DVLOG(2) << "pause(" << *this << ")";

  // When updating pause, be sure to update PauseToLetDescriptionFinish().
  autoplay_policy_->StopAutoplayMutedWhenVisible();
  PauseInternal(PlayPromiseError::kPaused_PauseCalled);
}

void HTMLMediaElement::PauseToLetDescriptionFinish() {
  DVLOG(2) << "pauseExceptSpeech(" << *this << ")";

  autoplay_policy_->StopAutoplayMutedWhenVisible();

  // Passing in pause_speech as false to pause everything except the speech.
  PauseInternal(PlayPromiseError::kPaused_PauseCalled, false);
}

void HTMLMediaElement::PauseInternal(PlayPromiseError code,
                                     bool pause_speech /* = true */) {
  DVLOG(3) << "pauseInternal(" << *this << ")";

  if (network_state_ == kNetworkEmpty)
    InvokeResourceSelectionAlgorithm();

  can_autoplay_ = false;

  if (!paused_) {
    paused_ = true;
    ScheduleTimeupdateEvent(false);
    ScheduleNamedEvent(event_type_names::kPause);

    // Force an update to official playback position. Automatic updates from
    // currentPlaybackPosition() will be blocked while paused_ = true. This
    // blocking is desired while paused, but its good to update it one final
    // time to accurately reflect movie time at the moment we paused.
    SetOfficialPlaybackPosition(CurrentPlaybackPosition());

    ScheduleRejectPlayPromises(code);
  }

  UpdatePlayState(pause_speech);
}

bool HTMLMediaElement::preservesPitch() const {
  return preserves_pitch_;
}

void HTMLMediaElement::setPreservesPitch(bool preserves_pitch) {
  preserves_pitch_ = preserves_pitch;

  if (web_media_player_)
    web_media_player_->SetPreservesPitch(preserves_pitch_);
}

double HTMLMediaElement::latencyHint() const {
  // Parse error will fallback to std::numeric_limits<double>::quiet_NaN()
  double seconds = GetFloatingPointAttribute(html_names::kLatencyhintAttr);

  // Return NaN for invalid values.
  if (!std::isfinite(seconds) || seconds < 0)
    return std::numeric_limits<double>::quiet_NaN();

  return seconds;
}

void HTMLMediaElement::setLatencyHint(double seconds) {
  SetFloatingPointAttribute(html_names::kLatencyhintAttr, seconds);
}

void HTMLMediaElement::FlingingStarted() {
  if (web_media_player_)
    web_media_player_->FlingingStarted();
}

void HTMLMediaElement::FlingingStopped() {
  if (web_media_player_)
    web_media_player_->FlingingStopped();
}

void HTMLMediaElement::CloseMediaSource() {
  if (!media_source_attachment_)
    return;

  media_source_attachment_->Close(media_source_tracer_);
  media_source_attachment_.reset();
  media_source_tracer_ = nullptr;
}

bool HTMLMediaElement::Loop() const {
  return FastHasAttribute(html_names::kLoopAttr);
}

void HTMLMediaElement::SetLoop(bool b) {
  DVLOG(3) << "setLoop(" << *this << ", " << BoolString(b) << ")";
  SetBooleanAttribute(html_names::kLoopAttr, b);
}

bool HTMLMediaElement::ShouldShowControls() const {
  // If the document is not active, then we should not show controls.
  if (!GetDocument().IsActive()) {
    return false;
  }

  Settings* settings = GetDocument().GetSettings();
  if (settings && !settings->GetMediaControlsEnabled()) {
    return false;
  }

  // If the user has explicitly shown or hidden the controls, then force that
  // choice.
  if (user_wants_controls_visible_.has_value()) {
    return *user_wants_controls_visible_;
  }

  if (FastHasAttribute(html_names::kControlsAttr) || IsFullscreen()) {
    return true;
  }

  ExecutionContext* context = GetExecutionContext();
  if (context && !context->CanExecuteScripts(kNotAboutToExecuteScript)) {
    return true;
  }
  return false;
}

bool HTMLMediaElement::ShouldShowAllControls() const {
  // If the user has explicitly shown or hidden the controls, then force that
  // choice. Otherwise returns whether controls should be shown and no controls
  // are meant to be hidden.
  return user_wants_controls_visible_.value_or(
      ShouldShowControls() && !ControlsListInternal()->CanShowAllControls());
}

DOMTokenList* HTMLMediaElement::controlsList() const {
  return controls_list_.Get();
}

HTMLMediaElementControlsList* HTMLMediaElement::ControlsListInternal() const {
  return controls_list_.Get();
}

double HTMLMediaElement::volume() const {
  return volume_;
}

void HTMLMediaElement::setVolume(double vol, ExceptionState& exception_state) {
  DVLOG(2) << "setVolume(" << *this << ", " << vol << ")";

  if (volume_ == vol)
    return;

  if (RuntimeEnabledFeatures::MediaElementVolumeGreaterThanOneEnabled()) {
    if (vol < 0.0f) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          ExceptionMessages::IndexExceedsMinimumBound("volume", vol, 0.0));
      return;
    }
  } else if (vol < 0.0f || vol > 1.0f) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange(
            "volume", vol, 0.0, ExceptionMessages::kInclusiveBound, 1.0,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  volume_ = vol;

  ScheduleNamedEvent(event_type_names::kVolumechange);

  // If it setting volume to audible and AutoplayPolicy doesn't want the
  // playback to continue, pause the playback.
  if (EffectiveMediaVolume() && !autoplay_policy_->RequestAutoplayUnmute())
    pause();

  // If playback was not paused by the autoplay policy and got audible, the
  // element is marked as being allowed to play unmuted.
  if (EffectiveMediaVolume() && PotentiallyPlaying())
    was_always_muted_ = false;

  if (web_media_player_)
    web_media_player_->SetVolume(EffectiveMediaVolume());

  autoplay_policy_->StopAutoplayMutedWhenVisible();
}

bool HTMLMediaElement::muted() const {
  return muted_;
}

void HTMLMediaElement::setMuted(bool muted) {
  DVLOG(2) << "setMuted(" << *this << ", " << BoolString(muted) << ")";

  if (muted_ == muted)
    return;

  muted_ = muted;

  ScheduleNamedEvent(event_type_names::kVolumechange);

  // If it is unmute and AutoplayPolicy doesn't want the playback to continue,
  // pause the playback.
  if (EffectiveMediaVolume() && !autoplay_policy_->RequestAutoplayUnmute())
    pause();

  // If playback was not paused by the autoplay policy and got unmuted, the
  // element is marked as being allowed to play unmuted.
  if (EffectiveMediaVolume() && PotentiallyPlaying())
    was_always_muted_ = false;

  // This is called at the end to make sure the WebMediaPlayer has the right
  // information.
  if (web_media_player_)
    web_media_player_->SetVolume(EffectiveMediaVolume());

  autoplay_policy_->StopAutoplayMutedWhenVisible();
}

void HTMLMediaElement::SetUserWantsControlsVisible(bool visible) {
  user_wants_controls_visible_ = visible;
  UpdateControlsVisibility();
}

bool HTMLMediaElement::UserWantsControlsVisible() const {
  return user_wants_controls_visible_.value_or(false);
}

double HTMLMediaElement::EffectiveMediaVolume() const {
  if (muted_)
    return 0;

  return volume_;
}

// The spec says to fire periodic timeupdate events (those sent while playing)
// every "15 to 250ms", we choose the slowest frequency
static const base::TimeDelta kMaxTimeupdateEventFrequency =
    base::Milliseconds(250);

void HTMLMediaElement::StartPlaybackProgressTimer() {
  if (playback_progress_timer_.IsActive())
    return;

  previous_progress_time_ = base::ElapsedTimer();
  playback_progress_timer_.StartRepeating(kMaxTimeupdateEventFrequency);
}

void HTMLMediaElement::PlaybackProgressTimerFired() {
  if (!std::isnan(fragment_end_time_) && currentTime() >= fragment_end_time_ &&
      GetDirectionOfPlayback() == kForward) {
    fragment_end_time_ = std::numeric_limits<double>::quiet_NaN();
    if (!paused_) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLMediaElementPauseAtFragmentEnd);
      // changes paused to true and fires a simple event named pause at the
      // media element.
      PauseInternal(PlayPromiseError::kPaused_EndOfPlayback);
    }
  }

  if (!seeking_)
    ScheduleTimeupdateEvent(true);

  // Playback progress is chosen here for simplicity as a proxy for a good
  // periodic time to also update the attached MediaSource, if any, with our
  // currentTime so that it can continue to have a "recent media time".
  ReportCurrentTimeToMediaSource();
}

void HTMLMediaElement::ScheduleTimeupdateEvent(bool periodic_event) {
  if (web_media_player_)
    web_media_player_->OnTimeUpdate();

  // Per spec, consult current playback position to check for changing time.
  double media_time = CurrentPlaybackPosition();
  bool media_time_has_progressed =
      std::isnan(last_time_update_event_media_time_)
          ? media_time != 0
          : media_time != last_time_update_event_media_time_;

  if (periodic_event && !media_time_has_progressed)
    return;

  ScheduleNamedEvent(event_type_names::kTimeupdate);

  last_time_update_event_media_time_ = media_time;

  // Restart the timer to ensure periodic event fires 250ms from _this_ event.
  if (!periodic_event && playback_progress_timer_.IsActive()) {
    playback_progress_timer_.Stop();
    playback_progress_timer_.StartRepeating(kMaxTimeupdateEventFrequency);
  }
}

void HTMLMediaElement::TogglePlayState() {
  if (paused())
    Play();
  else
    pause();
}

AudioTrackList& HTMLMediaElement::audioTracks() {
  return *audio_tracks_;
}

void HTMLMediaElement::AudioTrackChanged(AudioTrack* track) {
  DVLOG(3) << "audioTrackChanged(" << *this
           << ") trackId= " << String(track->id())
           << " enabled=" << BoolString(track->enabled())
           << " exclusive=" << BoolString(track->IsExclusive());

  if (track->enabled()) {
    audioTracks().TrackEnabled(track->id(), track->IsExclusive());
  }

  audioTracks().ScheduleChangeEvent();

  if (media_source_attachment_)
    media_source_attachment_->OnTrackChanged(media_source_tracer_, track);

  if (!audio_tracks_timer_.IsActive())
    audio_tracks_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void HTMLMediaElement::AudioTracksTimerFired(TimerBase*) {
  Vector<WebMediaPlayer::TrackId> enabled_track_ids;
  for (unsigned i = 0; i < audioTracks().length(); ++i) {
    AudioTrack* track = audioTracks().AnonymousIndexedGetter(i);
    if (track->enabled())
      enabled_track_ids.push_back(track->id());
  }

  web_media_player_->EnabledAudioTracksChanged(enabled_track_ids);
}

VideoTrackList& HTMLMediaElement::videoTracks() {
  return *video_tracks_;
}

void HTMLMediaElement::SelectedVideoTrackChanged(VideoTrack* track) {
  DVLOG(3) << "selectedVideoTrackChanged(" << *this << ") selectedTrackId="
           << (track->selected() ? String(track->id()) : "none");

  if (track->selected())
    videoTracks().TrackSelected(track->id());

  videoTracks().ScheduleChangeEvent();

  if (media_source_attachment_)
    media_source_attachment_->OnTrackChanged(media_source_tracer_, track);

  if (track->selected()) {
    web_media_player_->SelectedVideoTrackChanged(track->id());
  } else {
    web_media_player_->SelectedVideoTrackChanged(std::nullopt);
  }
}

void HTMLMediaElement::AddMediaTrack(const media::MediaTrack& track) {
  switch (track.type()) {
    case media::MediaTrack::Type::kVideo: {
      bool enabled = track.enabled() && videoTracks().selectedIndex() == -1;
      videoTracks().Add(MakeGarbageCollected<VideoTrack>(
          String::FromUTF8(track.track_id().value()),
          WebString::FromUTF8(track.kind().value()),
          WebString::FromUTF8(track.label().value()),
          WebString::FromUTF8(track.language().value()), enabled));
      break;
    }
    case media::MediaTrack::Type::kAudio: {
      audioTracks().Add(MakeGarbageCollected<AudioTrack>(
          String::FromUTF8(track.track_id().value()),
          WebString::FromUTF8(track.kind().value()),
          WebString::FromUTF8(track.label().value()),
          WebString::FromUTF8(track.language().value()), track.enabled(),
          track.exclusive()));
      break;
    }
  }
}

void HTMLMediaElement::RemoveMediaTrack(const media::MediaTrack& track) {
  switch (track.type()) {
    case media::MediaTrack::Type::kVideo: {
      videoTracks().Remove(String::FromUTF8(track.track_id().value()));
      break;
    }
    case media::MediaTrack::Type::kAudio: {
      audioTracks().Remove(String::FromUTF8(track.track_id().value()));
      break;
    }
  }
}

void HTMLMediaElement::ForgetResourceSpecificTracks() {
  audio_tracks_->RemoveAll();
  video_tracks_->RemoveAll();

  audio_tracks_timer_.Stop();
}

TextTrack* HTMLMediaElement::addTextTrack(const V8TextTrackKind& kind,
                                          const AtomicString& label,
                                          const AtomicString& language,
                                          ExceptionState& exception_state) {
  // https://html.spec.whatwg.org/C/#dom-media-addtexttrack

  // The addTextTrack(kind, label, language) method of media elements, when
  // invoked, must run the following steps:

  // 1. Create a new TextTrack object.
  // 2. Create a new text track corresponding to the new object, and set its
  //    text track kind to kind, its text track label to label, its text
  //    track language to language, ..., and its text track list of cues to
  //    an empty list.
  auto* text_track =
      MakeGarbageCollected<TextTrack>(kind, label, language, *this);
  //    ..., its text track readiness state to the text track loaded state, ...
  text_track->SetReadinessState(TextTrack::kLoaded);

  // 3. Add the new text track to the media element's list of text tracks.
  // 4. Queue a task to fire a trusted event with the name addtrack, that
  //    does not bubble and is not cancelable, and that uses the TrackEvent
  //    interface, with the track attribute initialised to the new text
  //    track's TextTrack object, at the media element's textTracks
  //    attribute's TextTrackList object.
  textTracks()->Append(text_track);

  // Note: Due to side effects when changing track parameters, we have to
  // first append the track to the text track list.
  // FIXME: Since setMode() will cause a 'change' event to be queued on the
  // same task source as the 'addtrack' event (see above), the order is
  // wrong. (The 'change' event shouldn't be fired at all in this case...)

  // ..., its text track mode to the text track hidden mode, ...
  text_track->SetModeEnum(TextTrackMode::kHidden);

  // 5. Return the new TextTrack object.
  return text_track;
}

TextTrackList* HTMLMediaElement::textTracks() {
  if (!text_tracks_) {
    UseCounter::Count(GetDocument(), WebFeature::kMediaElementTextTrackList);
    text_tracks_ = MakeGarbageCollected<TextTrackList>(this);
  }

  return text_tracks_.Get();
}

void HTMLMediaElement::DidAddTrackElement(HTMLTrackElement* track_element) {
  // 4.8.12.11.3 Sourcing out-of-band text tracks
  // When a track element's parent element changes and the new parent is a media
  // element, then the user agent must add the track element's corresponding
  // text track to the media element's list of text tracks ... [continues in
  // TextTrackList::append]
  TextTrack* text_track = track_element->track();
  if (!text_track)
    return;

  textTracks()->Append(text_track);

  // Do not schedule the track loading until parsing finishes so we don't start
  // before all tracks in the markup have been added.
  if (IsFinishedParsingChildren())
    ScheduleTextTrackResourceLoad();
}

void HTMLMediaElement::DidRemoveTrackElement(HTMLTrackElement* track_element) {
  KURL url = track_element->GetNonEmptyURLAttribute(html_names::kSrcAttr);
  DVLOG(3) << "didRemoveTrackElement(" << *this << ") - 'src' is "
           << UrlForLoggingMedia(url);

  TextTrack* text_track = track_element->track();
  if (!text_track)
    return;

  text_track->SetHasBeenConfigured(false);

  if (!text_tracks_)
    return;

  // 4.8.12.11.3 Sourcing out-of-band text tracks
  // When a track element's parent element changes and the old parent was a
  // media element, then the user agent must remove the track element's
  // corresponding text track from the media element's list of text tracks.
  text_tracks_->Remove(text_track);

  wtf_size_t index =
      text_tracks_when_resource_selection_began_.Find(text_track);
  if (index != kNotFound)
    text_tracks_when_resource_selection_began_.EraseAt(index);
}

void HTMLMediaElement::HonorUserPreferencesForAutomaticTextTrackSelection() {
  if (!text_tracks_ || !text_tracks_->length())
    return;

  if (!should_perform_automatic_track_selection_)
    return;

  AutomaticTrackSelection::Configuration configuration;
  if (processing_preference_change_)
    configuration.disable_currently_enabled_tracks = true;
  if (text_tracks_visible_)
    configuration.force_enable_subtitle_or_caption_track = true;

  Settings* settings = GetDocument().GetSettings();
  if (settings) {
    configuration.text_track_kind_user_preference =
        settings->GetTextTrackKindUserPreference();
  }

  AutomaticTrackSelection track_selection(configuration);
  track_selection.Perform(*text_tracks_);
}

bool HTMLMediaElement::HavePotentialSourceChild() {
  // Stash the current <source> node and next nodes so we can restore them after
  // checking to see there is another potential.
  HTMLSourceElement* current_source_node = current_source_node_;
  Node* next_node = next_child_node_to_consider_;

  KURL next_url = SelectNextSourceChild(nullptr, kDoNothing);

  current_source_node_ = current_source_node;
  next_child_node_to_consider_ = next_node;

  return next_url.IsValid();
}

KURL HTMLMediaElement::SelectNextSourceChild(
    String* content_type,
    InvalidURLAction action_if_invalid) {
  // Don't log if this was just called to find out if there are any valid
  // <source> elements.
  bool should_log = action_if_invalid != kDoNothing;
  if (should_log)
    DVLOG(3) << "selectNextSourceChild(" << *this << ")";

  if (!next_child_node_to_consider_) {
    if (should_log) {
      DVLOG(3) << "selectNextSourceChild(" << *this << ") -> 0x0000, \"\"";
    }
    return KURL();
  }

  KURL media_url;
  Node* node;
  HTMLSourceElement* source = nullptr;
  String type;
  bool looking_for_start_node = next_child_node_to_consider_ != nullptr;
  bool can_use_source_element = false;

  NodeVector potential_source_nodes;
  GetChildNodes(*this, potential_source_nodes);

  for (unsigned i = 0;
       !can_use_source_element && i < potential_source_nodes.size(); ++i) {
    node = potential_source_nodes[i].Get();
    if (looking_for_start_node && next_child_node_to_consider_ != node)
      continue;
    looking_for_start_node = false;

    source = DynamicTo<HTMLSourceElement>(node);
    if (!source || node->parentNode() != this) {
      continue;
    }

    // 2. If candidate does not have a src attribute, or if its src
    // attribute's value is the empty string ... jump down to the failed
    // step below
    const AtomicString& src_value =
        source->FastGetAttribute(html_names::kSrcAttr);
    if (should_log) {
      DVLOG(3) << "selectNextSourceChild(" << *this << ") - 'src' is "
               << UrlForLoggingMedia(media_url);
    }
    if (src_value.empty()) {
      goto checkAgain;
    }

    // 3. If candidate has a media attribute whose value does not match the
    // environment, then end the synchronous section, and jump down to the
    // failed with elements step below.
    if (!source->MediaQueryMatches() &&
        base::FeatureList::IsEnabled(kVideoSourceMediaQuerySupport)) {
      goto checkAgain;
    }

    // 4. Let urlRecord be the result of encoding-parsing a URL given
    // candidate's src attribute's value, relative to candidate's node document
    // when the src attribute was last changed.
    media_url = source->GetDocument().CompleteURL(src_value);

    // 5. If urlRecord is failure, then end the synchronous section, and jump
    // down to the failed with elements step below.
    if (!IsSafeToLoadURL(media_url, action_if_invalid)) {
      goto checkAgain;
    }

    // 6. If candidate has a type attribute whose value, when parsed as a
    // MIME type ...
    type = source->type();
    if (type.empty() && media_url.ProtocolIsData())
      type = MimeTypeFromDataURL(media_url);
    if (!type.empty()) {
      if (should_log) {
        DVLOG(3) << "selectNextSourceChild(" << *this << ") - 'type' is '"
                 << type << "'";
      }
      if (!GetSupportsType(ContentType(type)))
        goto checkAgain;
    }

    // Making it this far means the <source> looks reasonable.
    can_use_source_element = true;

  checkAgain:
    if (!can_use_source_element && action_if_invalid == kComplain && source)
      source->ScheduleErrorEvent();
  }

  if (can_use_source_element) {
    if (content_type)
      *content_type = type;
    current_source_node_ = source;
    next_child_node_to_consider_ = source->nextSibling();
  } else {
    current_source_node_ = nullptr;
    next_child_node_to_consider_ = nullptr;
  }

  if (should_log) {
    DVLOG(3) << "selectNextSourceChild(" << *this << ") -> "
             << current_source_node_.Get() << ", "
             << (can_use_source_element ? UrlForLoggingMedia(media_url) : "");
  }

  return can_use_source_element ? media_url : KURL();
}

void HTMLMediaElement::SourceWasAdded(HTMLSourceElement* source) {
  DVLOG(3) << "sourceWasAdded(" << *this << ", " << source << ")";

  KURL url = source->GetNonEmptyURLAttribute(html_names::kSrcAttr);
  DVLOG(3) << "sourceWasAdded(" << *this << ") - 'src' is "
           << UrlForLoggingMedia(url);

  // We should only consider a <source> element when there is not src attribute
  // at all.
  if (FastHasAttribute(html_names::kSrcAttr))
    return;

  // 4.8.8 - If a source element is inserted as a child of a media element that
  // has no src attribute and whose networkState has the value NETWORK_EMPTY,
  // the user agent must invoke the media element's resource selection
  // algorithm.
  if (getNetworkState() == HTMLMediaElement::kNetworkEmpty) {
    InvokeResourceSelectionAlgorithm();
    // Ignore current |next_child_node_to_consider_| and consider |source|.
    next_child_node_to_consider_ = source;
    return;
  }

  if (current_source_node_ && source == current_source_node_->nextSibling()) {
    DVLOG(3) << "sourceWasAdded(" << *this
             << ") - <source> inserted immediately after current source";
    // Ignore current |next_child_node_to_consider_| and consider |source|.
    next_child_node_to_consider_ = source;
    return;
  }

  // Consider current |next_child_node_to_consider_| as it is already in the
  // middle of processing.
  if (next_child_node_to_consider_)
    return;

  if (load_state_ != kWaitingForSource)
    return;

  // 4.8.9.5, resource selection algorithm, source elements section:
  // 21. Wait until the node after pointer is a node other than the end of the
  // list. (This step might wait forever.)
  // 22. Asynchronously await a stable state...
  // 23. Set the element's delaying-the-load-event flag back to true (this
  // delays the load event again, in case it hasn't been fired yet).
  SetShouldDelayLoadEvent(true);

  // 24. Set the networkState back to NETWORK_LOADING.
  // Changing the network state might trigger media controls to add new nodes
  // to the DOM which is forbidden while source is being inserted into this
  // node. This is a problem as ContainerNode::NotifyNodeInsertedInternal,
  // which is always indirectly triggering this function, prohibits event
  // dispatch and adding new nodes will run
  // blink::DispatchChildInsertionEvents.
  //
  // We still need to update the media controls. This will be done after
  // load_timer_ fires a new event - which is setup in ScheduleNextSourceChild
  // below so skipping that step here should be OK.
  SetNetworkState(kNetworkLoading, false /* update_media_controls */);

  // 25. Jump back to the find next candidate step above.
  next_child_node_to_consider_ = source;
  ScheduleNextSourceChild();
}

void HTMLMediaElement::SourceWasRemoved(HTMLSourceElement* source) {
  DVLOG(3) << "sourceWasRemoved(" << *this << ", " << source << ")";

  KURL url = source->GetNonEmptyURLAttribute(html_names::kSrcAttr);
  DVLOG(3) << "sourceWasRemoved(" << *this << ") - 'src' is "
           << UrlForLoggingMedia(url);

  if (source != current_source_node_ && source != next_child_node_to_consider_)
    return;

  if (source == next_child_node_to_consider_) {
    if (current_source_node_)
      next_child_node_to_consider_ = current_source_node_->nextSibling();
    DVLOG(3) << "sourceWasRemoved(" << *this
             << ") - next_child_node_to_consider_ set to "
             << next_child_node_to_consider_.Get();
  } else if (source == current_source_node_) {
    // Clear the current source node pointer, but don't change the movie as the
    // spec says:
    // 4.8.8 - Dynamically modifying a source element and its attribute when the
    // element is already inserted in a video or audio element will have no
    // effect.
    current_source_node_ = nullptr;
    DVLOG(3) << "SourceWasRemoved(" << *this
             << ") - current_source_node_ set to 0";
  }
}

void HTMLMediaElement::TimeChanged() {
  DVLOG(3) << "timeChanged(" << *this << ")";

  // 4.8.12.9 steps 12-14. Needed if no ReadyState change is associated with the
  // seek.
  if (seeking_ && ready_state_ >= kHaveCurrentData &&
      !web_media_player_->Seeking()) {
    FinishSeek();
  }

  // When the current playback position reaches the end of the media resource
  // when the direction of playback is forwards, then the user agent must follow
  // these steps:
  if (EndedPlayback(LoopCondition::kIgnored)) {
    // If the media element has a loop attribute specified
    if (Loop()) {
      //  then seek to the earliest possible position of the media resource and
      //  abort these steps.
      Seek(EarliestPossiblePosition());
    } else {
      // Queue a task to fire a simple event named timeupdate at the media
      // element.
      ScheduleTimeupdateEvent(false);

      // If the media element has still ended playback, and the direction of
      // playback is still forwards, and paused is false,
      if (!paused_) {
        // Trigger an update to `official_playback_position_` (if necessary)
        // BEFORE setting `paused_ = false`, to ensure a final sync with
        // `WebMediaPlayer()->CurrentPlaybackPosition()`.
        OfficialPlaybackPosition();

        // changes paused to true and fires a simple event named pause at the
        // media element.
        paused_ = true;
        ScheduleNamedEvent(event_type_names::kPause);
        ScheduleRejectPlayPromises(PlayPromiseError::kPaused_EndOfPlayback);
      }
      // Queue a task to fire a simple event named ended at the media element.
      ScheduleNamedEvent(event_type_names::kEnded);
    }
  }
  UpdatePlayState();
}

void HTMLMediaElement::DurationChanged() {
  DVLOG(3) << "durationChanged(" << *this << ")";

  // durationChanged() is triggered by media player.
  CHECK(web_media_player_);
  double new_duration = web_media_player_->Duration();

  // If the duration is changed such that the *current playback position* ends
  // up being greater than the time of the end of the media resource, then the
  // user agent must also seek to the time of the end of the media resource.
  DurationChanged(new_duration, CurrentPlaybackPosition() > new_duration);
}

void HTMLMediaElement::DurationChanged(double duration, bool request_seek) {
  DVLOG(3) << "durationChanged(" << *this << ", " << duration << ", "
           << BoolString(request_seek) << ")";

  // Abort if duration unchanged.
  if (duration_ == duration)
    return;

  DVLOG(3) << "durationChanged(" << *this << ") : " << duration_ << " -> "
           << duration;
  duration_ = duration;
  ScheduleNamedEvent(event_type_names::kDurationchange);

  if (web_media_player_)
    web_media_player_->OnTimeUpdate();

  UpdateLayoutObject();

  if (request_seek)
    Seek(duration);
}

void HTMLMediaElement::RemotePlaybackCompatibilityChanged(const WebURL& url,
                                                          bool is_compatible) {
  if (RemotePlaybackClient())
    RemotePlaybackClient()->SourceChanged(url, is_compatible);
}

bool HTMLMediaElement::HasSelectedVideoTrack() {
  return video_tracks_ && video_tracks_->selectedIndex() != -1;
}

WebMediaPlayer::TrackId HTMLMediaElement::GetSelectedVideoTrackId() {
  DCHECK(HasSelectedVideoTrack());

  int selected_track_index = video_tracks_->selectedIndex();
  VideoTrack* track =
      video_tracks_->AnonymousIndexedGetter(selected_track_index);
  return track->id();
}

bool HTMLMediaElement::WasAlwaysMuted() {
  return was_always_muted_;
}

// MediaPlayerPresentation methods
void HTMLMediaElement::Repaint() {
  if (cc_layer_)
    cc_layer_->SetNeedsDisplay();

  UpdateLayoutObject();
  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::SizeChanged() {
  DVLOG(3) << "sizeChanged(" << *this << ")";

  DCHECK(HasVideo());  // "resize" makes no sense in absence of video.
  if (ready_state_ > kHaveNothing && IsHTMLVideoElement())
    ScheduleNamedEvent(event_type_names::kResize);

  UpdateLayoutObject();
}

WebTimeRanges HTMLMediaElement::BufferedInternal() const {
  if (media_source_attachment_) {
    return media_source_attachment_->BufferedInternal(
        media_source_tracer_.Get());
  }

  if (!web_media_player_)
    return {};

  return web_media_player_->Buffered();
}

TimeRanges* HTMLMediaElement::buffered() const {
  return MakeGarbageCollected<TimeRanges>(BufferedInternal());
}

TimeRanges* HTMLMediaElement::played() {
  if (playing_) {
    double time = currentTime();
    if (time > last_seek_time_)
      AddPlayedRange(last_seek_time_, time);
  }

  if (!played_time_ranges_)
    played_time_ranges_ = MakeGarbageCollected<TimeRanges>();

  return played_time_ranges_->Copy();
}

WebTimeRanges HTMLMediaElement::SeekableInternal() const {
  if (!web_media_player_)
    return {};

  if (media_source_attachment_) {
    return media_source_attachment_->SeekableInternal(
        media_source_tracer_.Get());
  }

  return web_media_player_->Seekable();
}

TimeRanges* HTMLMediaElement::seekable() const {
  return MakeGarbageCollected<TimeRanges>(SeekableInternal());
}

bool HTMLMediaElement::PotentiallyPlaying() const {
  // Once we've reached the metadata state the WebMediaPlayer is ready to accept
  // play state changes.
  return ready_state_ >= kHaveMetadata && CouldPlayIfEnoughData();
}

bool HTMLMediaElement::CouldPlayIfEnoughData() const {
  return !paused() && !EndedPlayback() && !StoppedDueToErrors();
}

bool HTMLMediaElement::EndedPlayback(LoopCondition loop_condition) const {
  // If we have infinite duration, we'll never have played for long enough to
  // have ended playback.
  const double dur = duration();
  if (std::isnan(dur) || dur == std::numeric_limits<double>::infinity())
    return false;

  // 4.8.12.8 Playing the media resource

  // A media element is said to have ended playback when the element's
  // readyState attribute is HAVE_METADATA or greater,
  if (ready_state_ < kHaveMetadata)
    return false;

  DCHECK_EQ(GetDirectionOfPlayback(), kForward);
  if (web_media_player_) {
    return web_media_player_->IsEnded() &&
           (loop_condition == LoopCondition::kIgnored || !Loop() ||
            dur <= std::numeric_limits<double>::epsilon());
  }

  return false;
}

bool HTMLMediaElement::StoppedDueToErrors() const {
  if (ready_state_ >= kHaveMetadata && error_) {
    WebTimeRanges seekable_ranges = SeekableInternal();
    if (!seekable_ranges.Contain(currentTime()))
      return true;
  }

  return false;
}

void HTMLMediaElement::UpdatePlayState(bool pause_speech /* = true */) {
  bool is_playing = web_media_player_ && !web_media_player_->Paused();
  bool should_be_playing = PotentiallyPlaying();

  DVLOG(3) << "updatePlayState(" << *this
           << ") - shouldBePlaying = " << BoolString(should_be_playing)
           << ", isPlaying = " << BoolString(is_playing);

  if (should_be_playing && !muted_)
    was_always_muted_ = false;

  if (should_be_playing) {
    if (!is_playing) {
      // Set rate, muted before calling play in case they were set before the
      // media engine was setup.  The media engine should just stash the rate
      // and muted values since it isn't already playing.
      web_media_player_->SetRate(playbackRate());
      web_media_player_->SetVolume(EffectiveMediaVolume());
      web_media_player_->Play();
      if (::features::IsTextBasedAudioDescriptionEnabled())
        SpeechSynthesis()->Resume();

      // These steps should not be necessary, but if `play()` is called before
      // a source change, we may get into a state where `paused_ == false` and
      // `show_poster_flag_ == true`. My (cassew@google.com) interpretation of
      // the spec is that we should not be playing in this scenario.
      // https://crbug.com/633591
      SetShowPosterFlag(false);
      GetCueTimeline().InvokeTimeMarchesOn();
    }

    StartPlaybackProgressTimer();
    playing_ = true;
  } else {  // Should not be playing right now
    if (is_playing) {
      web_media_player_->Pause();

      if (pause_speech && ::features::IsTextBasedAudioDescriptionEnabled())
        SpeechSynthesis()->Pause();
    }

    playback_progress_timer_.Stop();
    playing_ = false;
    double time = currentTime();
    if (time > last_seek_time_)
      AddPlayedRange(last_seek_time_, time);

    GetCueTimeline().OnPause();
  }

  UpdateLayoutObject();

  if (web_media_player_)
    web_media_player_->OnTimeUpdate();

  ReportCurrentTimeToMediaSource();
  PseudoStateChanged(CSSSelector::kPseudoPaused);
  PseudoStateChanged(CSSSelector::kPseudoPlaying);

  UpdateVideoVisibilityTracker();
}

void HTMLMediaElement::StopPeriodicTimers() {
  progress_event_timer_.Stop();
  playback_progress_timer_.Stop();
  if (lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_->disconnect();
    lazy_load_intersection_observer_ = nullptr;
  }
}

void HTMLMediaElement::
    ClearMediaPlayerAndAudioSourceProviderClientWithoutLocking() {
  GetAudioSourceProvider().SetClient(nullptr);
  if (web_media_player_) {
    audio_source_provider_.Wrap(nullptr);
    web_media_player_.reset();
    // Do not clear `opener_document_` here; new players might still use it.

    // The lifetime of the mojo endpoints are tied to the WebMediaPlayer's, so
    // we need to reset those as well.
    media_player_receiver_set_->Value().Clear();
    media_player_observer_remote_set_->Value().Clear();
  }

  OnWebMediaPlayerCleared();
}

void HTMLMediaElement::ClearMediaPlayer() {
  ForgetResourceSpecificTracks();

  CloseMediaSource();

  CancelDeferredLoad();

  {
    AudioSourceProviderClientLockScope scope(*this);
    ClearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  }

  StopPeriodicTimers();
  load_timer_.Stop();

  pending_action_flags_ = 0;
  load_state_ = kWaitingForSource;

  if (GetLayoutObject())
    GetLayoutObject()->SetShouldDoFullPaintInvalidation();
}

void HTMLMediaElement::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kFrozenAutoResumeMedia && playing_) {
    paused_by_context_paused_ = true;
    pause();
    if (web_media_player_) {
      web_media_player_->OnFrozen();
    }
  } else if (state == mojom::FrameLifecycleState::kFrozen && playing_) {
    pause();
    if (web_media_player_) {
      web_media_player_->OnFrozen();
    }
  } else if (state == mojom::FrameLifecycleState::kRunning &&
             paused_by_context_paused_) {
    paused_by_context_paused_ = false;
    Play();
  }
}

void HTMLMediaElement::ContextDestroyed() {
  DVLOG(3) << "contextDestroyed(" << static_cast<void*>(this) << ")";

  // Close the async event queue so that no events are enqueued.
  CancelPendingEventsAndCallbacks();

  // Clear everything in the Media Element
  if (media_source_attachment_)
    media_source_attachment_->OnElementContextDestroyed();
  ClearMediaPlayer();
  ready_state_ = kHaveNothing;
  ready_state_maximum_ = kHaveNothing;
  SetNetworkState(kNetworkEmpty);
  SetShouldDelayLoadEvent(false);
  current_source_node_ = nullptr;
  official_playback_position_ = 0;
  official_playback_position_needs_update_ = true;
  playing_ = false;
  paused_ = true;
  seeking_ = false;
  GetCueTimeline().OnReadyStateReset();

  UpdateLayoutObject();

  StopPeriodicTimers();
  removed_from_document_timer_.Stop();

  UpdateVideoVisibilityTracker();
}

bool HTMLMediaElement::HasPendingActivity() const {
  const auto result = HasPendingActivityInternal();
  // TODO(dalecurtis): Replace c-style casts in followup patch.
  DVLOG(3) << "HasPendingActivity(" << *this << ") = " << result;
  return result;
}

bool HTMLMediaElement::HasPendingActivityInternal() const {
  // The delaying-the-load-event flag is set by resource selection algorithm
  // when looking for a resource to load, before networkState has reached to
  // kNetworkLoading.
  if (should_delay_load_event_)
    return true;

  // When networkState is kNetworkLoading, progress and stalled events may be
  // fired.
  //
  // When connected to a MediaSource, ignore |network_state_|. The rest
  // of this method's logic and the HasPendingActivity() of the various
  // MediaSource API objects more precisely indicate whether or not any pending
  // activity is expected on the group of connected HTMLMediaElement +
  // MediaSource API objects. This lets the group of objects be garbage
  // collected if there is no pending activity nor reachability from a GC root,
  // even while in kNetworkLoading.
  //
  // We use the WebMediaPlayer's network state instead of |network_state_| since
  // it's value is unreliable prior to ready state kHaveMetadata.
  if (!media_source_attachment_) {
    if (!web_media_player_) {
      if (network_state_ == kNetworkLoading)
        return true;
    } else if (web_media_player_->GetNetworkState() ==
               WebMediaPlayer::kNetworkStateLoading) {
      return true;
    }
  }

  {
    // Disable potential updating of playback position, as that will
    // require v8 allocations; not allowed while GCing
    // (hasPendingActivity() is called during a v8 GC.)
    base::AutoReset<bool> scope(&official_playback_position_needs_update_,
                                false);

    // When playing or if playback may continue, timeupdate events may be fired.
    if (CouldPlayIfEnoughData())
      return true;
  }

  // When the seek finishes timeupdate and seeked events will be fired.
  if (seeking_)
    return true;

  // Wait for any pending events to be fired.
  if (async_event_queue_->HasPendingEvents())
    return true;

  return false;
}

bool HTMLMediaElement::IsFullscreen() const {
  return Fullscreen::IsFullscreenElement(*this);
}

cc::Layer* HTMLMediaElement::CcLayer() const {
  return cc_layer_;
}

bool HTMLMediaElement::HasClosedCaptions() const {
  if (!text_tracks_)
    return false;

  for (unsigned i = 0; i < text_tracks_->length(); ++i) {
    if (text_tracks_->AnonymousIndexedGetter(i)->CanBeRendered())
      return true;
  }

  return false;
}

bool HTMLMediaElement::TextTracksVisible() const {
  return text_tracks_visible_;
}

// static
void HTMLMediaElement::AssertShadowRootChildren(ShadowRoot& shadow_root) {
#if DCHECK_IS_ON()
  // There can be up to three children: an interstitial (media remoting or
  // picture in picture), text track container, and media controls. The media
  // controls has to be the last child if present, and has to be the next
  // sibling of the text track container if both present. When present, media
  // remoting interstitial has to be the first child.
  unsigned number_of_children = shadow_root.CountChildren();
  DCHECK_LE(number_of_children, 3u);
  Node* first_child = shadow_root.firstChild();
  Node* last_child = shadow_root.lastChild();
  if (number_of_children == 1) {
    DCHECK(first_child->IsTextTrackContainer() ||
           first_child->IsMediaControls() ||
           first_child->IsMediaRemotingInterstitial() ||
           first_child->IsPictureInPictureInterstitial());
  } else if (number_of_children == 2) {
    DCHECK(first_child->IsTextTrackContainer() ||
           first_child->IsMediaRemotingInterstitial() ||
           first_child->IsPictureInPictureInterstitial());
    DCHECK(last_child->IsTextTrackContainer() || last_child->IsMediaControls());
    if (first_child->IsTextTrackContainer())
      DCHECK(last_child->IsMediaControls());
  } else if (number_of_children == 3) {
    Node* second_child = first_child->nextSibling();
    DCHECK(first_child->IsMediaRemotingInterstitial() ||
           first_child->IsPictureInPictureInterstitial());
    DCHECK(second_child->IsTextTrackContainer());
    DCHECK(last_child->IsMediaControls());
  }
#endif
}

TextTrackContainer& HTMLMediaElement::EnsureTextTrackContainer() {
  UseCounter::Count(GetDocument(), WebFeature::kMediaElementTextTrackContainer);

  ShadowRoot& shadow_root = EnsureUserAgentShadowRoot();
  AssertShadowRootChildren(shadow_root);

  Node* first_child = shadow_root.firstChild();
  if (auto* first_child_text_track = DynamicTo<TextTrackContainer>(first_child))
    return *first_child_text_track;
  Node* to_be_inserted = first_child;

  if (first_child && (first_child->IsMediaRemotingInterstitial() ||
                      first_child->IsPictureInPictureInterstitial())) {
    Node* second_child = first_child->nextSibling();
    if (auto* second_child_text_track =
            DynamicTo<TextTrackContainer>(second_child))
      return *second_child_text_track;
    to_be_inserted = second_child;
  }

  auto* text_track_container = MakeGarbageCollected<TextTrackContainer>(*this);

  // The text track container should be inserted before the media controls,
  // so that they are rendered behind them.
  shadow_root.InsertBefore(text_track_container, to_be_inserted);

  AssertShadowRootChildren(shadow_root);

  return *text_track_container;
}

void HTMLMediaElement::UpdateTextTrackDisplay() {
  DVLOG(3) << "updateTextTrackDisplay(" << *this << ")";

  EnsureTextTrackContainer().UpdateDisplay(
      *this, TextTrackContainer::kDidNotStartExposingControls);
}

SpeechSynthesisBase* HTMLMediaElement::SpeechSynthesis() {
  if (!speech_synthesis_) {
    speech_synthesis_ =
        SpeechSynthesisBase::Create(*(GetDocument().domWindow()));
    speech_synthesis_->SetOnSpeakingCompletedCallback(WTF::BindRepeating(
        &HTMLMediaElement::OnSpeakingCompleted, WrapWeakPersistent(this)));
  }
  return speech_synthesis_.Get();
}

void HTMLMediaElement::MediaControlsDidBecomeVisible() {
  DVLOG(3) << "mediaControlsDidBecomeVisible(" << *this << ")";

  // When the user agent starts exposing a user interface for a video element,
  // the user agent should run the rules for updating the text track rendering
  // of each of the text tracks in the video element's list of text tracks ...
  if (IsHTMLVideoElement() && TextTracksVisible()) {
    EnsureTextTrackContainer().UpdateDisplay(
        *this, TextTrackContainer::kDidStartExposingControls);
  }
}

void HTMLMediaElement::SetTextTrackKindUserPreferenceForAllMediaElements(
    Document* document) {
  auto it = DocumentToElementSetMap().find(document);
  if (it == DocumentToElementSetMap().end())
    return;
  DCHECK(it->value);
  WeakMediaElementSet& elements = *it->value;
  for (const auto& element : elements)
    element->AutomaticTrackSelectionForUpdatedUserPreference();
}

void HTMLMediaElement::AutomaticTrackSelectionForUpdatedUserPreference() {
  if (!text_tracks_ || !text_tracks_->length())
    return;

  MarkCaptionAndSubtitleTracksAsUnconfigured();
  processing_preference_change_ = true;
  text_tracks_visible_ = false;
  HonorUserPreferencesForAutomaticTextTrackSelection();
  processing_preference_change_ = false;

  // If a track is set to 'showing' post performing automatic track selection,
  // set text tracks state to visible to update the CC button and display the
  // track.
  text_tracks_visible_ = text_tracks_->HasShowingTracks();
  UpdateTextTrackDisplay();
}

void HTMLMediaElement::MarkCaptionAndSubtitleTracksAsUnconfigured() {
  if (!text_tracks_)
    return;

  // Mark all tracks as not "configured" so that
  // honorUserPreferencesForAutomaticTextTrackSelection() will reconsider
  // which tracks to display in light of new user preferences (e.g. default
  // tracks should not be displayed if the user has turned off captions and
  // non-default tracks should be displayed based on language preferences if
  // the user has turned captions on).
  for (unsigned i = 0; i < text_tracks_->length(); ++i) {
    TextTrack* text_track = text_tracks_->AnonymousIndexedGetter(i);
    if (text_track->IsVisualKind())
      text_track->SetHasBeenConfigured(false);
  }
}

uint64_t HTMLMediaElement::webkitAudioDecodedByteCount() const {
  if (!web_media_player_)
    return 0;
  return web_media_player_->AudioDecodedByteCount();
}

uint64_t HTMLMediaElement::webkitVideoDecodedByteCount() const {
  if (!web_media_player_)
    return 0;
  return web_media_player_->VideoDecodedByteCount();
}

bool HTMLMediaElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLMediaElement::SetShouldDelayLoadEvent(bool should_delay) {
  if (should_delay_load_event_ == should_delay)
    return;

  DVLOG(3) << "setShouldDelayLoadEvent(" << *this << ", "
           << BoolString(should_delay) << ")";

  should_delay_load_event_ = should_delay;
  if (should_delay)
    GetDocument().IncrementLoadEventDelayCount();
  else
    GetDocument().DecrementLoadEventDelayCount();
}

MediaControls* HTMLMediaElement::GetMediaControls() const {
  return media_controls_.Get();
}

void HTMLMediaElement::EnsureMediaControls() {
  if (GetMediaControls())
    return;

  ShadowRoot& shadow_root = EnsureUserAgentShadowRoot();
  UseCounterMuteScope scope(*this);
  media_controls_ =
      CoreInitializer::GetInstance().CreateMediaControls(*this, shadow_root);

  // The media controls should be inserted after the text track container,
  // so that they are rendered in front of captions and subtitles. This check
  // is verifying the contract.
  AssertShadowRootChildren(shadow_root);
}

void HTMLMediaElement::UpdateControlsVisibility() {
  if (!isConnected())
    return;

  bool native_controls = ShouldShowControls();

  // When LazyInitializeMediaControls is enabled, initialize the controls only
  // if native controls should be used or if using the cast overlay.
  if (!RuntimeEnabledFeatures::LazyInitializeMediaControlsEnabled() ||
      RuntimeEnabledFeatures::MediaCastOverlayButtonEnabled() ||
      native_controls) {
    EnsureMediaControls();

    // TODO(mlamouri): this doesn't sound needed but the following tests, on
    // Android fails when removed:
    // fullscreen/compositor-touch-hit-rects-fullscreen-video-controls.html
    GetMediaControls()->Reset();
  }

  if (native_controls)
    GetMediaControls()->MaybeShow();
  else if (GetMediaControls())
    GetMediaControls()->Hide();

  if (web_media_player_)
    web_media_player_->OnHasNativeControlsChanged(native_controls);
}

CueTimeline& HTMLMediaElement::GetCueTimeline() {
  if (!cue_timeline_)
    cue_timeline_ = MakeGarbageCollected<CueTimeline>(*this);
  return *cue_timeline_;
}

void HTMLMediaElement::ConfigureTextTrackDisplay() {
  DCHECK(text_tracks_);
  DVLOG(3) << "configureTextTrackDisplay(" << *this << ")";

  if (processing_preference_change_)
    return;

  bool have_visible_text_track = text_tracks_->HasShowingTracks();
  text_tracks_visible_ = have_visible_text_track;

  if (!have_visible_text_track && !GetMediaControls())
    return;

  // Note: The "time marches on" algorithm |CueTimeline::TimeMarchesOn| runs
  // the "rules for updating the text track rendering" (updateTextTrackDisplay)
  // only for "affected tracks", i.e. tracks where the the active cues have
  // changed. This misses cues in tracks that changed mode between hidden and
  // showing. This appears to be a spec bug, which we work around here:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=28236
  UpdateTextTrackDisplay();
}

// TODO(srirama.m): Merge it to resetMediaElement if possible and remove it.
void HTMLMediaElement::ResetMediaPlayerAndMediaSource() {
  CloseMediaSource();

  {
    AudioSourceProviderClientLockScope scope(*this);
    ClearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  }

  if (audio_source_node_)
    GetAudioSourceProvider().SetClient(audio_source_node_);
}

void HTMLMediaElement::SetAudioSourceNode(
    AudioSourceProviderClient* source_node) {
  DCHECK(IsMainThread());
  audio_source_node_ = source_node;

  // No need to lock the |audio_source_node| because it locks itself when
  // setFormat() is invoked.
  GetAudioSourceProvider().SetClient(audio_source_node_);
}

WebMediaPlayer::CorsMode HTMLMediaElement::CorsMode() const {
  const AtomicString& cross_origin_mode =
      FastGetAttribute(html_names::kCrossoriginAttr);
  if (cross_origin_mode.IsNull())
    return WebMediaPlayer::kCorsModeUnspecified;
  if (EqualIgnoringASCIICase(cross_origin_mode, "use-credentials"))
    return WebMediaPlayer::kCorsModeUseCredentials;
  return WebMediaPlayer::kCorsModeAnonymous;
}

void HTMLMediaElement::SetCcLayer(cc::Layer* cc_layer) {
  if (cc_layer == cc_layer_)
    return;

  SetNeedsCompositingUpdate();
  cc_layer_ = cc_layer;
}

void HTMLMediaElement::MediaSourceOpened(
    std::unique_ptr<WebMediaSource> web_media_source) {
  SetShouldDelayLoadEvent(false);
  media_source_attachment_->CompleteAttachingToMediaElement(
      media_source_tracer_, std::move(web_media_source));
}

bool HTMLMediaElement::IsInteractiveContent() const {
  return FastHasAttribute(html_names::kControlsAttr);
}

void HTMLMediaElement::BindMediaPlayerReceiver(
    mojo::PendingAssociatedReceiver<media::mojom::blink::MediaPlayer>
        receiver) {
  media_player_receiver_set_->Value().Add(
      std::move(receiver),
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
}

void HTMLMediaElement::OnSpeakingCompleted() {
  if (paused())
    Play();
}

void HTMLMediaElement::Trace(Visitor* visitor) const {
  visitor->Trace(audio_source_node_);
  visitor->Trace(speech_synthesis_);
  visitor->Trace(load_timer_);
  visitor->Trace(audio_tracks_timer_);
  visitor->Trace(removed_from_document_timer_);
  visitor->Trace(played_time_ranges_);
  visitor->Trace(async_event_queue_);
  visitor->Trace(error_);
  visitor->Trace(current_source_node_);
  visitor->Trace(next_child_node_to_consider_);
  visitor->Trace(deferred_load_timer_);
  visitor->Trace(media_source_tracer_);
  visitor->Trace(audio_tracks_);
  visitor->Trace(video_tracks_);
  visitor->Trace(cue_timeline_);
  visitor->Trace(text_tracks_);
  visitor->Trace(text_tracks_when_resource_selection_began_);
  visitor->Trace(play_promise_resolvers_);
  visitor->Trace(play_promise_resolve_list_);
  visitor->Trace(play_promise_reject_list_);
  visitor->Trace(audio_source_provider_);
  visitor->Trace(src_object_stream_descriptor_);
  visitor->Trace(src_object_media_source_handle_);
  visitor->Trace(autoplay_policy_);
  visitor->Trace(media_controls_);
  visitor->Trace(controls_list_);
  visitor->Trace(lazy_load_intersection_observer_);
  visitor->Trace(media_player_host_remote_);
  visitor->Trace(media_player_observer_remote_set_);
  visitor->Trace(media_player_receiver_set_);
  visitor->Trace(opener_document_);
  visitor->Trace(opener_context_observer_);
  Supplementable<HTMLMediaElement>::Trace(visitor);
  HTMLElement::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void HTMLMediaElement::CreatePlaceholderTracksIfNecessary() {
  // Create a placeholder audio track if the player says it has audio but it
  // didn't explicitly announce the tracks.
  if (HasAudio() && !audioTracks().length()) {
    AddMediaTrack(media::MediaTrack::CreateAudioTrack(
        "audio", media::MediaTrack::AudioKind::kMain, "Audio Track", "", true));
  }

  // Create a placeholder video track if the player says it has video but it
  // didn't explicitly announce the tracks.
  if (HasVideo() && !videoTracks().length()) {
    AddMediaTrack(media::MediaTrack::CreateVideoTrack(
        "video", media::MediaTrack::VideoKind::kMain, "Video Track", "", true));
  }
}

void HTMLMediaElement::SetNetworkState(NetworkState state,
                                       bool update_media_controls) {
  if (network_state_ == state)
    return;

  network_state_ = state;
  if (update_media_controls && GetMediaControls())
    GetMediaControls()->NetworkStateChanged();
}

void HTMLMediaElement::VideoWillBeDrawnToCanvas() const {
  DCHECK(IsHTMLVideoElement());
  UseCounter::Count(GetDocument(), WebFeature::kVideoInCanvas);
  autoplay_policy_->VideoWillBeDrawnToCanvas();
}

void HTMLMediaElement::ScheduleResolvePlayPromises() {
  // TODO(mlamouri): per spec, we should create a new task but we can't create
  // a new cancellable task without cancelling the previous one. There are two
  // approaches then: cancel the previous task and create a new one with the
  // appended promise list or append the new promise to the current list. The
  // latter approach is preferred because it might be the less observable
  // change.
  DCHECK(play_promise_resolve_list_.empty() ||
         play_promise_resolve_task_handle_.IsActive());
  if (play_promise_resolvers_.empty())
    return;

  play_promise_resolve_list_.AppendVector(play_promise_resolvers_);
  play_promise_resolvers_.clear();

  if (play_promise_resolve_task_handle_.IsActive())
    return;

  play_promise_resolve_task_handle_ = PostCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      WTF::BindOnce(&HTMLMediaElement::ResolveScheduledPlayPromises,
                    WrapWeakPersistent(this)));
}

void HTMLMediaElement::ScheduleRejectPlayPromises(PlayPromiseError code) {
  // TODO(mlamouri): per spec, we should create a new task but we can't create
  // a new cancellable task without cancelling the previous one. There are two
  // approaches then: cancel the previous task and create a new one with the
  // appended promise list or append the new promise to the current list. The
  // latter approach is preferred because it might be the less observable
  // change.
  DCHECK(play_promise_reject_list_.empty() ||
         play_promise_reject_task_handle_.IsActive());
  if (play_promise_resolvers_.empty())
    return;

  play_promise_reject_list_.AppendVector(play_promise_resolvers_);
  play_promise_resolvers_.clear();

  if (play_promise_reject_task_handle_.IsActive())
    return;

  // TODO(nhiroki): Bind this error code to a cancellable task instead of a
  // member field.
  play_promise_error_code_ = code;
  play_promise_reject_task_handle_ = PostCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kMediaElementEvent), FROM_HERE,
      WTF::BindOnce(&HTMLMediaElement::RejectScheduledPlayPromises,
                    WrapWeakPersistent(this)));
}

void HTMLMediaElement::ScheduleNotifyPlaying() {
  ScheduleNamedEvent(event_type_names::kPlaying);
  ScheduleResolvePlayPromises();
}

void HTMLMediaElement::ResolveScheduledPlayPromises() {
  for (auto& resolver : play_promise_resolve_list_)
    resolver->DowncastTo<IDLUndefined>()->Resolve();

  play_promise_resolve_list_.clear();
}

void HTMLMediaElement::RejectScheduledPlayPromises() {
  if (play_promise_error_code_ == PlayPromiseError::kNotSupported) {
    RejectPlayPromisesInternal(
        DOMExceptionCode::kNotSupportedError,
        "Failed to load because no supported source was found.");
    return;
  }

  const char* reason = "";
  switch (play_promise_error_code_) {
    case PlayPromiseError::kPaused_Unknown:
      reason = " because the media paused";
      break;
    case PlayPromiseError::kPaused_PauseCalled:
      reason = " by a call to pause()";
      break;
    case PlayPromiseError::kPaused_EndOfPlayback:
      reason = " by end of playback";
      break;
    case PlayPromiseError::kPaused_RemovedFromDocument:
      reason = " because the media was removed from the document";
      break;
    case PlayPromiseError::kPaused_AutoplayAutoPause:
      reason = " because autoplaying background media was paused to save power";
      break;
    case PlayPromiseError::kPaused_PageHidden:
      reason = " because video-only background media was paused to save power";
      break;
    case PlayPromiseError::kPaused_SuspendedPlayerIdleTimeout:
      reason = " because the player was been suspended and became idle";
      break;
    case PlayPromiseError::kPaused_RemotePlayStateChange:
      reason = " by a pause request from a remote media player";
      break;
    case PlayPromiseError::kPaused_PauseRequestedByUser:
      reason = " because a pause was requested by the user";
      break;
    case PlayPromiseError::kPaused_PauseRequestedInternally:
      reason = " because a pause was requested by the browser";
      break;
    case PlayPromiseError::kPaused_FrameHidden:
      reason =
          " because the media playback is not allowed by the "
          "media-playback-while-not-visible permission policy";
      break;
    case PlayPromiseError::kNotSupported:
      NOTREACHED_IN_MIGRATION();
  }
  RejectPlayPromisesInternal(
      DOMExceptionCode::kAbortError,
      String::Format(
          "The play() request was interrupted%s. https://goo.gl/LdLk22",
          reason));
}

void HTMLMediaElement::RejectPlayPromises(DOMExceptionCode code,
                                          const String& message) {
  play_promise_reject_list_.AppendVector(play_promise_resolvers_);
  play_promise_resolvers_.clear();
  RejectPlayPromisesInternal(code, message);
}

void HTMLMediaElement::RejectPlayPromisesInternal(DOMExceptionCode code,
                                                  const String& message) {
  DCHECK(code == DOMExceptionCode::kAbortError ||
         code == DOMExceptionCode::kNotSupportedError);
  for (auto& resolver : play_promise_reject_list_)
    resolver->Reject(MakeGarbageCollected<DOMException>(code, message));

  play_promise_reject_list_.clear();
}

void HTMLMediaElement::OnRemovedFromDocumentTimerFired(TimerBase*) {
  if (InActiveDocument())
    return;

  // Video should not pause when playing in Picture-in-Picture and subsequently
  // removed from the Document.
  if (!PictureInPictureController::IsElementInPictureInPicture(this))
    PauseInternal(PlayPromiseError::kPaused_RemovedFromDocument);
}

void HTMLMediaElement::AudioSourceProviderImpl::Wrap(
    scoped_refptr<WebAudioSourceProviderImpl> provider) {
  base::AutoLock locker(provide_input_lock);

  if (web_audio_source_provider_ && provider != web_audio_source_provider_)
    web_audio_source_provider_->SetClient(nullptr);

  web_audio_source_provider_ = std::move(provider);
  if (web_audio_source_provider_)
    web_audio_source_provider_->SetClient(client_.Get());
}

void HTMLMediaElement::AudioSourceProviderImpl::SetClient(
    AudioSourceProviderClient* client) {
  base::AutoLock locker(provide_input_lock);

  if (client)
    client_ = MakeGarbageCollected<HTMLMediaElement::AudioClientImpl>(client);
  else
    client_.Clear();

  if (web_audio_source_provider_)
    web_audio_source_provider_->SetClient(client_.Get());
}

void HTMLMediaElement::AudioSourceProviderImpl::ProvideInput(
    AudioBus* bus,
    int frames_to_process) {
  DCHECK(bus);

  base::AutoTryLock try_locker(provide_input_lock);
  if (!try_locker.is_acquired() || !web_audio_source_provider_ ||
      !client_.Get()) {
    bus->Zero();
    return;
  }

  // Wrap the AudioBus channel data using WebVector.
  unsigned n = bus->NumberOfChannels();
  WebVector<float*> web_audio_data(n);
  for (unsigned i = 0; i < n; ++i)
    web_audio_data[i] = bus->Channel(i)->MutableData();

  web_audio_source_provider_->ProvideInput(web_audio_data, frames_to_process);
}

void HTMLMediaElement::AudioClientImpl::SetFormat(uint32_t number_of_channels,
                                                  float sample_rate) {
  if (client_)
    client_->SetFormat(number_of_channels, sample_rate);
}

void HTMLMediaElement::AudioClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

void HTMLMediaElement::AudioSourceProviderImpl::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
}

bool HTMLMediaElement::HasNativeControls() {
  return ShouldShowControls();
}

bool HTMLMediaElement::IsAudioElement() {
  return IsHTMLAudioElement();
}

DisplayType HTMLMediaElement::GetDisplayType() const {
  return IsFullscreen() ? DisplayType::kFullscreen : DisplayType::kInline;
}

gfx::ColorSpace HTMLMediaElement::TargetColorSpace() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return gfx::ColorSpace();
  return frame->GetPage()
      ->GetChromeClient()
      .GetScreenInfo(*frame)
      .display_color_spaces.GetScreenInfoColorSpace();
}

bool HTMLMediaElement::WasAutoplayInitiated() {
  return autoplay_policy_->WasAutoplayInitiated();
}

void HTMLMediaElement::ResumePlayback() {
  autoplay_policy_->EnsureAutoplayInitiatedSet();
  PlayInternal();
}

void HTMLMediaElement::PausePlayback(PauseReason pause_reason) {
  switch (pause_reason) {
    case PauseReason::kUnknown:
      return PauseInternal(PlayPromiseError::kPaused_Unknown);
    case PauseReason::kPageHidden:
      return PauseInternal(PlayPromiseError::kPaused_PageHidden);
    case PauseReason::kSuspendedPlayerIdleTimeout:
      return PauseInternal(
          PlayPromiseError::kPaused_SuspendedPlayerIdleTimeout);
    case PauseReason::kRemotePlayStateChange:
      return PauseInternal(PlayPromiseError::kPaused_RemotePlayStateChange);
    case PauseReason::kFrameHidden:
      return PauseInternal(PlayPromiseError::kPaused_FrameHidden);
  }
  NOTREACHED_IN_MIGRATION();
}

void HTMLMediaElement::DidPlayerStartPlaying() {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnMediaPlaying();
}

void HTMLMediaElement::DidPlayerPaused(bool stream_ended) {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnMediaPaused(stream_ended);
}

void HTMLMediaElement::DidPlayerMutedStatusChange(bool muted) {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnMutedStatusChanged(muted);
}

void HTMLMediaElement::DidMediaMetadataChange(
    bool has_audio,
    bool has_video,
    media::AudioCodec audio_codec,
    media::VideoCodec video_codec,
    media::MediaContentType media_content_type,
    bool is_encrypted_media) {
  for (auto& observer : media_player_observer_remote_set_->Value()) {
    observer->OnMediaMetadataChanged(has_audio, has_video, media_content_type);
  }

  video_codec_ = has_video ? std::make_optional(video_codec) : std::nullopt;
  audio_codec_ = has_audio ? std::make_optional(audio_codec) : std::nullopt;

  is_encrypted_media_ = is_encrypted_media;
  OnRemotePlaybackMetadataChange();
}

void HTMLMediaElement::DidPlayerMediaPositionStateChange(
    double playback_rate,
    base::TimeDelta duration,
    base::TimeDelta position,
    bool end_of_media) {
  for (auto& observer : media_player_observer_remote_set_->Value()) {
    observer->OnMediaPositionStateChanged(
        media_session::mojom::blink::MediaPosition::New(
            playback_rate, duration, position, base::TimeTicks::Now(),
            end_of_media));
  }
}

void HTMLMediaElement::DidDisableAudioOutputSinkChanges() {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnAudioOutputSinkChangingDisabled();
}

void HTMLMediaElement::DidUseAudioServiceChange(bool uses_audio_service) {
  for (auto& observer : media_player_observer_remote_set_->Value()) {
    observer->OnUseAudioServiceChanged(uses_audio_service);
  }
}

void HTMLMediaElement::DidPlayerSizeChange(const gfx::Size& size) {
  for (auto& observer : media_player_observer_remote_set_->Value())
    observer->OnMediaSizeChanged(size);
}

void HTMLMediaElement::OnRemotePlaybackDisabled(bool disabled) {
  if (is_remote_playback_disabled_ == disabled)
    return;
  is_remote_playback_disabled_ = disabled;
  OnRemotePlaybackMetadataChange();
}

media::mojom::blink::MediaPlayerHost&
HTMLMediaElement::GetMediaPlayerHostRemote() {
  // It is an error to call this before having access to the document's frame.
  DCHECK(GetDocument().GetFrame());
  if (!media_player_host_remote_->Value().is_bound()) {
    GetDocument()
        .GetFrame()
        ->GetRemoteNavigationAssociatedInterfaces()
        ->GetInterface(
            media_player_host_remote_->Value().BindNewEndpointAndPassReceiver(
                GetDocument().GetTaskRunner(TaskType::kInternalMedia)));
  }
  return *media_player_host_remote_->Value().get();
}

mojo::PendingAssociatedReceiver<media::mojom::blink::MediaPlayerObserver>
HTMLMediaElement::AddMediaPlayerObserverAndPassReceiver() {
  mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayerObserver>
      observer;
  auto observer_receiver = observer.InitWithNewEndpointAndPassReceiver();
  media_player_observer_remote_set_->Value().Add(
      std::move(observer),
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  return observer_receiver;
}

void HTMLMediaElement::RequestPlay() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (frame) {
    LocalFrame::NotifyUserActivation(
        frame, mojom::blink::UserActivationNotificationType::kInteraction);
  }
  autoplay_policy_->EnsureAutoplayInitiatedSet();
  PlayInternal();
}

void HTMLMediaElement::RequestPause(bool triggered_by_user) {
  if (triggered_by_user) {
    LocalFrame* frame = GetDocument().GetFrame();
    if (frame) {
      LocalFrame::NotifyUserActivation(
          frame, mojom::blink::UserActivationNotificationType::kInteraction);
    }
  }
  PauseInternal(triggered_by_user
                    ? PlayPromiseError::kPaused_PauseRequestedByUser
                    : PlayPromiseError::kPaused_PauseRequestedInternally);
}

void HTMLMediaElement::RequestSeekForward(base::TimeDelta seek_time) {
  double seconds = seek_time.InSecondsF();
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  setCurrentTime(currentTime() + seconds);
}

void HTMLMediaElement::RequestSeekBackward(base::TimeDelta seek_time) {
  double seconds = seek_time.InSecondsF();
  DCHECK_GE(seconds, 0) << "Attempted to seek by a negative number of seconds";
  setCurrentTime(currentTime() - seconds);
}

void HTMLMediaElement::RequestSeekTo(base::TimeDelta seek_time) {
  setCurrentTime(seek_time.InSecondsF());
}

void HTMLMediaElement::RequestMute(bool mute) {
  setMuted(mute);
}

void HTMLMediaElement::SetVolumeMultiplier(double multiplier) {
  if (web_media_player_)
    web_media_player_->SetVolumeMultiplier(multiplier);
}

void HTMLMediaElement::SetPowerExperimentState(bool enabled) {
  if (web_media_player_)
    web_media_player_->SetPowerExperimentState(enabled);
}

void HTMLMediaElement::SetAudioSinkId(const String& sink_id) {
  auto* audio_output_controller = AudioOutputDeviceController::From(*this);
  DCHECK(audio_output_controller);

  audio_output_controller->SetSinkId(sink_id);
}

void HTMLMediaElement::SuspendForFrameClosed() {
  if (web_media_player_)
    web_media_player_->SuspendForFrameClosed();
}

bool HTMLMediaElement::MediaShouldBeOpaque() const {
  return !IsMediaDataCorsSameOrigin() && ready_state_ < kHaveMetadata &&
         EffectivePreloadType() != WebMediaPlayer::kPreloadNone;
}

void HTMLMediaElement::SetError(MediaError* error) {
  error_ = error;

  if (error) {
    DLOG(ERROR) << __func__ << ": {code=" << error->code()
                << ", message=" << error->message() << "}";
    if (media_source_attachment_)
      media_source_attachment_->OnElementError();
  }
}

void HTMLMediaElement::ReportCurrentTimeToMediaSource() {
  if (!media_source_attachment_)
    return;

  // See MediaSourceAttachment::OnElementTimeUpdate() for why the attachment
  // needs our currentTime.
  media_source_attachment_->OnElementTimeUpdate(currentTime());
}

void HTMLMediaElement::OnRemotePlaybackMetadataChange() {
  if (remote_playback_client_) {
    remote_playback_client_->MediaMetadataChanged(video_codec_, audio_codec_);
  }
  for (auto& observer : media_player_observer_remote_set_->Value()) {
    observer->OnRemotePlaybackMetadataChange(
        media_session::mojom::blink::RemotePlaybackMetadata::New(
            WTF::String(media::GetCodecName(video_codec_
                                                ? video_codec_.value()
                                                : media::VideoCodec::kUnknown)),
            WTF::String(media::GetCodecName(audio_codec_
                                                ? audio_codec_.value()
                                                : media::AudioCodec::kUnknown)),
            is_remote_playback_disabled_, is_remote_rendering_,
            WTF::String(remote_device_friendly_name_), is_encrypted_media_));
  }
}

HTMLMediaElement::OpenerContextObserver::OpenerContextObserver(
    HTMLMediaElement* element)
    : element_(element) {}

HTMLMediaElement::OpenerContextObserver::~OpenerContextObserver() = default;

void HTMLMediaElement::OpenerContextObserver::Trace(Visitor* visitor) const {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(element_);
}

void HTMLMediaElement::OpenerContextObserver::ContextDestroyed() {
  element_->AttachToNewFrame();
}

STATIC_ASSERT_ENUM(WebMediaPlayer::kReadyStateHaveNothing,
                   HTMLMediaElement::kHaveNothing);
STATIC_ASSERT_ENUM(WebMediaPlayer::kReadyStateHaveMetadata,
                   HTMLMediaElement::kHaveMetadata);
STATIC_ASSERT_ENUM(WebMediaPlayer::kReadyStateHaveCurrentData,
                   HTMLMediaElement::kHaveCurrentData);
STATIC_ASSERT_ENUM(WebMediaPlayer::kReadyStateHaveFutureData,
                   HTMLMediaElement::kHaveFutureData);
STATIC_ASSERT_ENUM(WebMediaPlayer::kReadyStateHaveEnoughData,
                   HTMLMediaElement::kHaveEnoughData);

}  // namespace blink
