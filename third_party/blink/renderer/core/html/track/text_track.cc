/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/text_track.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/cue_timeline.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

static const int kInvalidTrackIndex = -1;

const AtomicString& TextTrack::SubtitlesKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, subtitles, ("subtitles"));
  return subtitles;
}

const AtomicString& TextTrack::CaptionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, captions, ("captions"));
  return captions;
}

const AtomicString& TextTrack::DescriptionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, descriptions, ("descriptions"));
  return descriptions;
}

const AtomicString& TextTrack::ChaptersKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, chapters, ("chapters"));
  return chapters;
}

const AtomicString& TextTrack::MetadataKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, metadata, ("metadata"));
  return metadata;
}

const AtomicString& TextTrack::DisabledKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, disabled, ("disabled"));
  return disabled;
}

const AtomicString& TextTrack::HiddenKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, hidden, ("hidden"));
  return hidden;
}

const AtomicString& TextTrack::ShowingKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, showing, ("showing"));
  return showing;
}

TextTrack::TextTrack(const AtomicString& kind,
                     const AtomicString& label,
                     const AtomicString& language,
                     const AtomicString& id,
                     TextTrackType type)
    : TrackBase(WebMediaPlayer::kTextTrack, kind, label, language, id),
      active_cues_(nullptr),
      track_list_(nullptr),
      mode_(DisabledKeyword()),
      track_type_(type),
      readiness_state_(kNotLoaded),
      track_index_(kInvalidTrackIndex),
      rendered_track_index_(kInvalidTrackIndex),
      has_been_configured_(false) {}

TextTrack::~TextTrack() = default;

bool TextTrack::IsValidKindKeyword(const String& value) {
  if (value == SubtitlesKeyword())
    return true;
  if (value == CaptionsKeyword())
    return true;
  if (value == DescriptionsKeyword())
    return true;
  if (value == ChaptersKeyword())
    return true;
  if (value == MetadataKeyword())
    return true;

  return false;
}

void TextTrack::SetTrackList(TextTrackList* track_list) {
  if (!track_list && GetCueTimeline() && cues_)
    GetCueTimeline()->RemoveCues(this, cues_.Get());

  track_list_ = track_list;
  InvalidateTrackIndex();
}

bool TextTrack::IsVisualKind() const {
  return kind() == SubtitlesKeyword() || kind() == CaptionsKeyword();
}

void TextTrack::setMode(const AtomicString& mode) {
  DCHECK(mode == DisabledKeyword() || mode == HiddenKeyword() ||
         mode == ShowingKeyword());

  // On setting, if the new value isn't equal to what the attribute would
  // currently return, the new value must be processed as follows ...
  if (mode_ == mode)
    return;

  if (cues_ && GetCueTimeline()) {
    // If mode changes to disabled, remove this track's cues from the client
    // because they will no longer be accessible from the cues() function.
    if (mode == DisabledKeyword())
      GetCueTimeline()->RemoveCues(this, cues_.Get());
    else if (mode != ShowingKeyword())
      GetCueTimeline()->HideCues(this, cues_.Get());
  }

  mode_ = mode;

  if (mode != DisabledKeyword() && GetReadinessState() == kLoaded) {
    if (cues_ && GetCueTimeline())
      GetCueTimeline()->AddCues(this, cues_.Get());
  }

  if (MediaElement())
    MediaElement()->TextTrackModeChanged(this);
}

TextTrackCueList* TextTrack::cues() {
  // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
  // then the cues attribute must return a live TextTrackCueList object ...
  // Otherwise, it must return null. When an object is returned, the
  // same object must be returned each time.
  // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-cues
  if (mode_ != DisabledKeyword())
    return EnsureTextTrackCueList();
  return nullptr;
}

void TextTrack::Reset() {
  if (!cues_)
    return;

  if (GetCueTimeline())
    GetCueTimeline()->RemoveCues(this, cues_.Get());

  for (wtf_size_t i = 0; i < cues_->length(); ++i)
    cues_->AnonymousIndexedGetter(i)->SetTrack(nullptr);

  cues_->RemoveAll();
  if (active_cues_)
    active_cues_->RemoveAll();

  style_sheets_.clear();
}

void TextTrack::AddListOfCues(
    HeapVector<Member<TextTrackCue>>& list_of_new_cues) {
  TextTrackCueList* cues = EnsureTextTrackCueList();

  for (auto& new_cue : list_of_new_cues) {
    new_cue->SetTrack(this);
    cues->Add(new_cue);
  }

  if (GetCueTimeline() && mode() != DisabledKeyword())
    GetCueTimeline()->AddCues(this, cues);
}

TextTrackCueList* TextTrack::activeCues() {
  // 4.8.10.12.5 If the text track mode ... is not the text track disabled mode,
  // then the activeCues attribute must return a live TextTrackCueList object
  // ... whose active flag was set when the script started, in text track cue
  // order. Otherwise, it must return null. When an object is returned, the same
  // object must be returned each time.
  // http://www.whatwg.org/specs/web-apps/current-work/#dom-texttrack-activecues
  if (!cues_ || mode_ == DisabledKeyword())
    return nullptr;

  if (!active_cues_) {
    active_cues_ = MakeGarbageCollected<TextTrackCueList>();
  }

  cues_->CollectActiveCues(*active_cues_);
  return active_cues_;
}

void TextTrack::addCue(TextTrackCue* cue) {
  DCHECK(cue);

  if (std::isnan(cue->startTime()) || std::isnan(cue->endTime()))
    return;

  // https://html.spec.whatwg.org/C/#dom-texttrack-addcue

  // The addCue(cue) method of TextTrack objects, when invoked, must run the
  // following steps:

  // (Steps 1 and 2 - pertaining to association of rendering rules - are not
  // implemented.)

  // 3. If the given cue is in a text track list of cues, then remove cue
  // from that text track list of cues.
  if (TextTrack* cue_track = cue->track())
    cue_track->removeCue(cue, ASSERT_NO_EXCEPTION);

  // 4. Add cue to the method's TextTrack object's text track's text track list
  // of cues.
  cue->SetTrack(this);
  EnsureTextTrackCueList()->Add(cue);

  if (GetCueTimeline() && mode_ != DisabledKeyword())
    GetCueTimeline()->AddCue(this, cue);
}

void TextTrack::SetCSSStyleSheets(
    HeapVector<Member<CSSStyleSheet>> style_sheets) {
  DCHECK(style_sheets_.IsEmpty());
  style_sheets_ = std::move(style_sheets);
}

void TextTrack::removeCue(TextTrackCue* cue, ExceptionState& exception_state) {
  DCHECK(cue);

  // https://html.spec.whatwg.org/C/#dom-texttrack-removecue

  // The removeCue(cue) method of TextTrack objects, when invoked, must run the
  // following steps:

  // 1. If the given cue is not currently listed in the method's TextTrack
  // object's text track's text track list of cues, then throw a NotFoundError
  // exception.
  if (cue->track() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The specified cue is not listed in the TextTrack's list of cues.");
    return;
  }

  // cue->track() == this implies that cue is in this track's list of cues,
  // so this track should have a list of cues and the cue being removed
  // should be in it.
  DCHECK(cues_);

  // 2. Remove cue from the method's TextTrack object's text track's text track
  // list of cues.
  bool was_removed = cues_->Remove(cue);
  DCHECK(was_removed);

  // If the cue is active, a timeline needs to be available.
  DCHECK(!cue->IsActive() || GetCueTimeline());

  cue->SetTrack(nullptr);

  if (GetCueTimeline())
    GetCueTimeline()->RemoveCue(this, cue);
}

void TextTrack::CueWillChange(TextTrackCue* cue) {
  // The cue may need to be repositioned in the media element's interval tree,
  // may need to be re-rendered, etc, so remove it before the modification...
  if (GetCueTimeline())
    GetCueTimeline()->RemoveCue(this, cue);
}

void TextTrack::CueDidChange(TextTrackCue* cue, bool update_cue_index) {
  // This method is called through cue->track(), which should imply that this
  // track has a list of cues.
  DCHECK(cues_ && cue->track() == this);

  // Make sure the TextTrackCueList order is up to date.
  if (update_cue_index)
    cues_->UpdateCueIndex(cue);

  // Since a call to cueDidChange is always preceded by a call to
  // cueWillChange, the cue should no longer be active when we reach this
  // point (since it was removed from the timeline in cueWillChange).
  DCHECK(!cue->IsActive());

  if (mode_ == DisabledKeyword())
    return;

  // ... and add it back again if the track is enabled.
  if (GetCueTimeline())
    GetCueTimeline()->AddCue(this, cue);
}

int TextTrack::TrackIndex() {
  DCHECK(track_list_);

  if (track_index_ == kInvalidTrackIndex)
    track_index_ = track_list_->GetTrackIndex(this);

  return track_index_;
}

void TextTrack::InvalidateTrackIndex() {
  track_index_ = kInvalidTrackIndex;
  rendered_track_index_ = kInvalidTrackIndex;
}

bool TextTrack::IsRendered() const {
  return mode_ == ShowingKeyword() && IsVisualKind();
}

bool TextTrack::CanBeRendered() const {
  // A track can be displayed when it's of kind captions or subtitles and hasn't
  // failed to load.
  return GetReadinessState() != kFailedToLoad && IsVisualKind();
}

TextTrackCueList* TextTrack::EnsureTextTrackCueList() {
  if (!cues_) {
    cues_ = MakeGarbageCollected<TextTrackCueList>();
  }

  return cues_.Get();
}

int TextTrack::TrackIndexRelativeToRenderedTracks() {
  DCHECK(track_list_);

  if (rendered_track_index_ == kInvalidTrackIndex)
    rendered_track_index_ =
        track_list_->GetTrackIndexRelativeToRenderedTracks(this);

  return rendered_track_index_;
}

const AtomicString& TextTrack::InterfaceName() const {
  return event_target_names::kTextTrack;
}

ExecutionContext* TextTrack::GetExecutionContext() const {
  HTMLMediaElement* owner = MediaElement();
  return owner ? owner->GetExecutionContext() : nullptr;
}

HTMLMediaElement* TextTrack::MediaElement() const {
  return track_list_ ? track_list_->Owner() : nullptr;
}

CueTimeline* TextTrack::GetCueTimeline() const {
  return MediaElement() ? &MediaElement()->GetCueTimeline() : nullptr;
}

Node* TextTrack::Owner() const {
  return MediaElement();
}

void TextTrack::Trace(Visitor* visitor) {
  visitor->Trace(cues_);
  visitor->Trace(active_cues_);
  visitor->Trace(track_list_);
  visitor->Trace(style_sheets_);
  TrackBase::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
