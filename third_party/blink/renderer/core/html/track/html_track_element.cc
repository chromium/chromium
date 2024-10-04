/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/track/html_track_element.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/loadable_text_track.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

#define TRACK_LOG_LEVEL 3

namespace blink {

static String UrlForLoggingTrack(const KURL& url) {
  static const unsigned kMaximumURLLengthForLogging = 128;

  const String& url_string = url.GetString();
  if (url_string.length() < kMaximumURLLengthForLogging) {
    return url_string;
  }
  return url_string.Substring(0, kMaximumURLLengthForLogging) + "...";
}

HTMLTrackElement::HTMLTrackElement(Document& document)
    : HTMLElement(html_names::kTrackTag, document),
      load_timer_(document.GetTaskRunner(TaskType::kNetworking),
                  this,
                  &HTMLTrackElement::LoadTimerFired) {
  DVLOG(TRACK_LOG_LEVEL) << "HTMLTrackElement - " << (void*)this;
}

HTMLTrackElement::~HTMLTrackElement() = default;

Node::InsertionNotificationRequest HTMLTrackElement::InsertedInto(
    ContainerNode& insertion_point) {
  DVLOG(TRACK_LOG_LEVEL) << "insertedInto";

  // Since we've moved to a new parent, we may now be able to load.
  ScheduleLoad();

  HTMLElement::InsertedInto(insertion_point);
  HTMLMediaElement* parent = MediaElement();
  if (&insertion_point == parent)
    parent->DidAddTrackElement(this);
  return kInsertionDone;
}

void HTMLTrackElement::RemovedFrom(ContainerNode& insertion_point) {
  auto* html_media_element = DynamicTo<HTMLMediaElement>(insertion_point);
  if (html_media_element && !parentNode())
    html_media_element->DidRemoveTrackElement(this);
  HTMLElement::RemovedFrom(insertion_point);
}

void HTMLTrackElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kSrcAttr) {
    ScheduleLoad();

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // As the kind, label, and srclang attributes are set, changed, or removed,
    // the text track must update accordingly...
  } else if (name == html_names::kKindAttr) {
    std::optional<V8TextTrackKind> kind;
    AtomicString lower_case_value = params.new_value.LowerASCII();
    // 'missing value default' ("subtitles")
    if (lower_case_value.IsNull()) {
      // 'missing value default' ("subtitles")
      kind = V8TextTrackKind(V8TextTrackKind::Enum::kSubtitles);
    } else {
      kind = V8TextTrackKind::Create(lower_case_value);
      if (!kind.has_value()) {
        kind = V8TextTrackKind(V8TextTrackKind::Enum::kMetadata);
      }
    }
    track()->SetKind(kind.value());
  } else if (name == html_names::kLabelAttr) {
    track()->SetLabel(params.new_value);
  } else if (name == html_names::kSrclangAttr) {
    track()->SetLanguage(params.new_value);
  } else if (name == html_names::kIdAttr) {
    track()->SetId(params.new_value);
  }

  HTMLElement::ParseAttribute(params);
}

AtomicString HTMLTrackElement::kind() {
  return track()->kind().AsAtomicString();
}

void HTMLTrackElement::setKind(const AtomicString& kind) {
  setAttribute(html_names::kKindAttr, kind);
}

LoadableTextTrack* HTMLTrackElement::EnsureTrack() {
  if (!track_) {
    // kind, label and language are updated by parseAttribute
    track_ = MakeGarbageCollected<LoadableTextTrack>(this);
  }
  return track_.Get();
}

TextTrack* HTMLTrackElement::track() {
  return EnsureTrack();
}

bool HTMLTrackElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

void HTMLTrackElement::ScheduleLoad() {
  DVLOG(TRACK_LOG_LEVEL) << "scheduleLoad";

  // 1. If another occurrence of this algorithm is already running for this text
  // track and its track element, abort these steps, letting that other
  // algorithm take care of this element.
  if (load_timer_.IsActive())
    return;

  // 2. If the text track's text track mode is not set to one of hidden or
  // showing, abort these steps.
  if (EnsureTrack()->mode() != TextTrackMode::kHidden &&
      EnsureTrack()->mode() != TextTrackMode::kShowing)
    return;

  // 3. If the text track's track element does not have a media element as a
  // parent, abort these steps.
  if (!MediaElement())
    return;

  // 4. Run the remainder of these steps in parallel, allowing whatever caused
  // these steps to run to continue.
  load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 5. Top: Await a stable state. The synchronous section consists of the
  // following steps. (The steps in the synchronous section are marked with [X])
  // FIXME: We use a timer to approximate a "stable state" - i.e. this is not
  // 100% per spec.
}

void HTMLTrackElement::LoadTimerFired(TimerBase*) {
  DVLOG(TRACK_LOG_LEVEL) << "loadTimerFired";

  // 7. [X] Let URL be the track URL of the track element.
  KURL url = GetNonEmptyURLAttribute(html_names::kSrcAttr);

  // Whenever a track element has its src attribute set, changed,
  // or removed, the user agent must immediately empty the
  // element's text track's text track list of cues.
  // Currently there are no other implementations clearing cues
  // list _immediately_, so we are trying to align with what they are
  // doing and remove cues as part of the synchronous section.
  // Also we will first check if the new URL is not equal with
  // the previous URL (there is an unclarified issue in spec
  // about it, see: https://github.com/whatwg/html/issues/2916)
  if (url == url_ && getReadyState() != ReadyState::kNone)
    return;

  if (track_)
    track_->Reset();

  url_ = url;

  // 6. [X] Set the text track readiness state to loading.
  // Step 7 does not depend on step 6, so they were reordered to grant
  // setting kLoading state after the equality check
  SetReadyState(ReadyState::kLoading);

  // 8. [X] If the track element's parent is a media element then let CORS mode
  // be the state of the parent media element's crossorigin content attribute.
  // Otherwise, let CORS mode be No CORS.
  const CrossOriginAttributeValue cors_mode =
      GetCrossOriginAttributeValue(MediaElementCrossOriginAttribute());

  // 9. End the synchronous section, continuing the remaining steps in parallel.

  // 10. If URL is not the empty string, perform a potentially CORS-enabled
  // fetch of URL, with the mode being CORS mode, the origin being the origin of
  // the track element's node document, and the default origin behaviour set to
  // fail.
  if (!CanLoadUrl(url)) {
    DidCompleteLoad(kFailure);
    return;
  }

  // 10. ... (continued) If, while fetching is ongoing, either:
  //
  //  * the track URL changes so that it is no longer equal to URL, while the
  //    text track mode is set to hidden or showing; or
  //
  //  * the text track mode changes to hidden or showing, while the track URL
  //    is not equal to URL
  //
  // ...then the user agent must abort fetching, discarding any pending tasks
  // generated by that algorithm (and in particular, not adding any cues to the
  // text track list of cues after the moment the URL changed), and then queue
  // an element task on the DOM manipulation task source given the track
  // element that first changes the text track readiness state to failed to
  // load and then fires an event named error at the track element.
  if (loader_)
    DidCompleteLoad(kFailure);

  loader_ =
      MakeGarbageCollected<TextTrackLoader, TextTrackLoaderClient&, Document&>(
          *this, GetDocument());
  if (!loader_->Load(url_, cors_mode))
    DidCompleteLoad(kFailure);
}

bool HTMLTrackElement::CanLoadUrl(const KURL& url) {
  HTMLMediaElement* parent = MediaElement();
  if (!parent || !GetExecutionContext())
    return false;

  if (url.IsEmpty())
    return false;

  if (!GetExecutionContext()->GetContentSecurityPolicy()->AllowMediaFromSource(
          url)) {
    DVLOG(TRACK_LOG_LEVEL) << "canLoadUrl(" << UrlForLoggingTrack(url)
                           << ") -> rejected by Content Security Policy";
    return false;
  }

  return true;
}

void HTMLTrackElement::DidCompleteLoad(LoadStatus status) {
  // If we have an associated loader, then detach from that.
  if (loader_) {
    loader_->Detach();
    loader_ = nullptr;
  }

  // 10. ... (continued)

  // If the fetching algorithm fails for any reason (network error, the server
  // returns an error code, a cross-origin check fails, etc), or if URL is the
  // empty string, then queue a task to first change the text track readiness
  // state to failed to load and then fire a simple event named error at the
  // track element. This task must use the DOM manipulation task source.
  //
  // (Note: We don't "queue a task" here because this method will only be called
  // from a timer - load_timer_ or TextTrackLoader::cue_load_timer_ - which
  // should be a reasonable, and hopefully non-observable, approximation of the
  // spec text. I.e we could consider this to be run from the "networking task
  // source".)
  //
  // If the fetching algorithm does not fail, but the type of the resource is
  // not a supported text track format, or the file was not successfully
  // processed (e.g. the format in question is an XML format and the file
  // contained a well-formedness error that the XML specification requires be
  // detected and reported to the application), then the task that is queued by
  // the networking task source in which the aforementioned problem is found
  // must change the text track readiness state to failed to load and fire a
  // simple event named error at the track element.
  if (status == kFailure) {
    SetReadyState(ReadyState::kError);
    DispatchEvent(*Event::Create(event_type_names::kError));
    return;
  }

  // If the fetching algorithm does not fail, and the file was successfully
  // processed, then the final task that is queued by the networking task
  // source, after it has finished parsing the data, must change the text track
  // readiness state to loaded, and fire a simple event named load at the track
  // element.
  SetReadyState(ReadyState::kLoaded);
  DispatchEvent(*Event::Create(event_type_names::kLoad));
}

void HTMLTrackElement::NewCuesAvailable(TextTrackLoader* loader) {
  DCHECK_EQ(loader_, loader);
  DCHECK(track_);

  HeapVector<Member<TextTrackCue>> new_cues;
  loader_->GetNewCues(new_cues);

  HeapVector<Member<CSSStyleSheet>> new_sheets;
  loader_->GetNewStyleSheets(new_sheets);

  if (!new_sheets.empty()) {
    track_->SetCSSStyleSheets(std::move(new_sheets));
  }

  track_->AddListOfCues(new_cues);
}

void HTMLTrackElement::CueLoadingCompleted(TextTrackLoader* loader,
                                           bool loading_failed) {
  DCHECK_EQ(loader_, loader);

  DidCompleteLoad(loading_failed ? kFailure : kSuccess);
}

// NOTE: The values in the TextTrack::ReadinessState enum must stay in sync with
// those in HTMLTrackElement::ReadyState.
static_assert(
    HTMLTrackElement::ReadyState::kNone ==
        static_cast<HTMLTrackElement::ReadyState>(TextTrack::kNotLoaded),
    "HTMLTrackElement::kNone should be in sync with TextTrack::NotLoaded");
static_assert(
    HTMLTrackElement::ReadyState::kLoading ==
        static_cast<HTMLTrackElement::ReadyState>(TextTrack::kLoading),
    "HTMLTrackElement::kLoading should be in sync with TextTrack::Loading");
static_assert(
    HTMLTrackElement::ReadyState::kLoaded ==
        static_cast<HTMLTrackElement::ReadyState>(TextTrack::kLoaded),
    "HTMLTrackElement::kLoaded should be in sync with TextTrack::Loaded");
static_assert(
    HTMLTrackElement::ReadyState::kError ==
        static_cast<HTMLTrackElement::ReadyState>(TextTrack::kFailedToLoad),
    "HTMLTrackElement::kError should be in sync with TextTrack::FailedToLoad");

void HTMLTrackElement::SetReadyState(ReadyState state) {
  EnsureTrack()->SetReadinessState(
      static_cast<TextTrack::ReadinessState>(state));
  if (HTMLMediaElement* parent = MediaElement())
    return parent->TextTrackReadyStateChanged(track_.Get());
}

HTMLTrackElement::ReadyState HTMLTrackElement::getReadyState() {
  return track_ ? static_cast<ReadyState>(track_->GetReadinessState())
                : ReadyState::kNone;
}

const AtomicString& HTMLTrackElement::MediaElementCrossOriginAttribute() const {
  if (HTMLMediaElement* parent = MediaElement())
    return parent->FastGetAttribute(html_names::kCrossoriginAttr);

  return g_null_atom;
}

HTMLMediaElement* HTMLTrackElement::MediaElement() const {
  return DynamicTo<HTMLMediaElement>(parentElement());
}

void HTMLTrackElement::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  visitor->Trace(loader_);
  visitor->Trace(load_timer_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
