// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/track/cue_timeline.h"

#include <algorithm>
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/html_track_element.h"
#include "third_party/blink/renderer/core/html/track/loadable_text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue_list.h"
#include "ui/accessibility/accessibility_features.h"

namespace blink {

namespace {

CueInterval CreateCueInterval(TextTrackCue* cue) {
  // Negative duration cues need be treated in the interval tree as
  // zero-length cues.
  double const interval_end_time = std::max(cue->startTime(), cue->endTime());
  return CueIntervalTree::CreateInterval(cue->startTime(), interval_end_time,
                                         cue);
}

base::TimeDelta CalculateEventTimeout(double event_time,
                                      HTMLMediaElement const& media_element) {
  static_assert(HTMLMediaElement::kMinPlaybackRate >= 0,
                "The following code assumes playback rates are never negative");
  DCHECK_NE(media_element.playbackRate(), 0);

  auto const timeout =
      base::Seconds((event_time - media_element.currentTime()) /
                    media_element.playbackRate());

  // Only allow timeouts of multiples of 1ms to prevent "polling-by-timer"
  // and excessive calls to `TimeMarchesOn`.
  constexpr base::TimeDelta kMinTimeoutInterval = base::Milliseconds(1);
  return std::max(timeout.CeilToMultiple(kMinTimeoutInterval),
                  kMinTimeoutInterval);
}

}  // namespace

CueTimeline::CueTimeline(HTMLMediaElement& media_element)
    : media_element_(&media_element),
      last_update_time_(-1),
      cue_event_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &CueTimeline::CueEventTimerFired),
      cue_timestamp_event_timer_(
          media_element.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &CueTimeline::CueTimestampEventTimerFired),
      ignore_update_(0),
      update_requested_while_ignoring_(false) {}

void CueTimeline::AddCues(TextTrack* track, const TextTrackCueList* cues) {
  DCHECK_NE(track->mode(), TextTrackMode::kDisabled);
  for (wtf_size_t i = 0; i < cues->length(); ++i)
    AddCueInternal(cues->AnonymousIndexedGetter(i));
  if (!MediaElement().IsShowPosterFlagSet()) {
    InvokeTimeMarchesOn();
  }
}

void CueTimeline::AddCue(TextTrack* track, TextTrackCue* cue) {
  DCHECK_NE(track->mode(), TextTrackMode::kDisabled);
  AddCueInternal(cue);
  if (!MediaElement().IsShowPosterFlagSet()) {
    InvokeTimeMarchesOn();
  }
}

void CueTimeline::AddCueInternal(TextTrackCue* cue) {
  CueInterval interval = CreateCueInterval(cue);
  if (!cue_tree_.Contains(interval))
    cue_tree_.Add(interval);
}

void CueTimeline::RemoveCues(TextTrack*, const TextTrackCueList* cues) {
  for (wtf_size_t i = 0; i < cues->length(); ++i)
    RemoveCueInternal(cues->AnonymousIndexedGetter(i));
  if (!MediaElement().IsShowPosterFlagSet()) {
    InvokeTimeMarchesOn();
  }
}

void CueTimeline::RemoveCue(TextTrack*, TextTrackCue* cue) {
  RemoveCueInternal(cue);
  if (!MediaElement().IsShowPosterFlagSet()) {
    InvokeTimeMarchesOn();
  }
}

void CueTimeline::RemoveCueInternal(TextTrackCue* cue) {
  CueInterval interval = CreateCueInterval(cue);
  cue_tree_.Remove(interval);

  wtf_size_t index = currently_active_cues_.Find(interval);
  if (index != kNotFound) {
    DCHECK(cue->IsActive());
    currently_active_cues_.EraseAt(index);
    cue->SetIsActive(false);
    // Since the cue will be removed from the media element and likely the
    // TextTrack might also be destructed, notifying the region of the cue
    // removal shouldn't be done.
    cue->RemoveDisplayTree(TextTrackCue::kDontNotifyRegion);
  }
}

void CueTimeline::HideCues(TextTrack*, const TextTrackCueList* cues) {
  for (wtf_size_t i = 0; i < cues->length(); ++i)
    cues->AnonymousIndexedGetter(i)->RemoveDisplayTree();
}

static bool TrackIndexCompare(TextTrack* a, TextTrack* b) {
  return a->TrackIndex() - b->TrackIndex() < 0;
}

static bool EventTimeCueCompare(const std::pair<double, TextTrackCue*>& a,
                                const std::pair<double, TextTrackCue*>& b) {
  // 12 - Sort the tasks in events in ascending time order (tasks with earlier
  // times first).
  if (a.first != b.first)
    return a.first - b.first < 0;

  // If the cues belong to different text tracks, it doesn't make sense to
  // compare the two tracks by the relative cue order, so return the relative
  // track order.
  if (a.second->track() != b.second->track())
    return TrackIndexCompare(a.second->track(), b.second->track());

  // 12 - Further sort tasks in events that have the same time by the
  // relative text track cue order of the text track cues associated
  // with these tasks.
  return a.second->CueIndex() < b.second->CueIndex();
}

static Event* CreateEventWithTarget(const AtomicString& event_name,
                                    EventTarget* event_target) {
  Event* event = Event::Create(event_name);
  event->SetTarget(event_target);
  return event;
}

void CueTimeline::TimeMarchesOn() {
  DCHECK(!MediaElement().IsShowPosterFlagSet());

  // 4.8.10.8 Playing the media resource

  //  If the current playback position changes while the steps are running,
  //  then the user agent must wait for the steps to complete, and then must
  //  immediately rerun the steps.
  if (InsideIgnoreUpdateScope()) {
    update_requested_while_ignoring_ = true;
    return;
  }

  // Prevent recursive updates
  auto scope = BeginIgnoreUpdateScope();

  HTMLMediaElement& media_element = MediaElement();
  double const movie_time = media_element.currentTime();

  // Don't run the "time marches on" algorithm if the document has been
  // detached. This primarily guards against dispatch of events w/
  // HTMLTrackElement targets.
  if (media_element.GetDocument().IsDetached())
    return;

  // Get the next cue event after this update
  next_cue_event_ = cue_tree_.NextIntervalPoint(movie_time);

  // https://html.spec.whatwg.org/C/#time-marches-on

  // 1 - Let current cues be a list of cues, initialized to contain all the
  // cues of all the hidden, showing, or showing by default text tracks of the
  // media element (not the disabled ones) whose start times are less than or
  // equal to the current playback position and whose end times are greater
  // than the current playback position.
  CueList current_cues;

  // The user agent must synchronously unset [the text track cue active] flag
  // whenever ... the media element's readyState is changed back to
  // kHaveNothing.
  if (media_element.getReadyState() != HTMLMediaElement::kHaveNothing &&
      media_element.GetWebMediaPlayer()) {
    current_cues =
        cue_tree_.AllOverlaps(cue_tree_.CreateInterval(movie_time, movie_time));
  }

  CueList previous_cues;

  // 2 - Let other cues be a list of cues, initialized to contain all the cues
  // of hidden, showing, and showing by default text tracks of the media
  // element that are not present in current cues.
  previous_cues = currently_active_cues_;

  // 3 - Let last time be the current playback position at the time this
  // algorithm was last run for this media element, if this is not the first
  // time it has run.
  double last_time = last_update_time_;
  double last_seek_time = media_element.LastSeekTime();

  // 4 - If the current playback position has, since the last time this
  // algorithm was run, only changed through its usual monotonic increase
  // during normal playback, then let missed cues be the list of cues in other
  // cues whose start times are greater than or equal to last time and whose
  // end times are less than or equal to the current playback position.
  // Otherwise, let missed cues be an empty list.
  CueList missed_cues;
  if (last_time >= 0 && last_seek_time < movie_time) {
    CueList potentially_skipped_cues =
        cue_tree_.AllOverlaps(cue_tree_.CreateInterval(last_time, movie_time));
    missed_cues.ReserveInitialCapacity(potentially_skipped_cues.size());

    for (CueInterval cue : potentially_skipped_cues) {
      // Consider cues that may have been missed since the last seek time.
      if (cue.Low() > std::max(last_seek_time, last_time) &&
          cue.High() < movie_time)
        missed_cues.push_back(cue);
    }
  }

  last_update_time_ = movie_time;

  // 5 - If the time was reached through the usual monotonic increase of the
  // current playback position during normal playback, and if the user agent
  // has not fired a timeupdate event at the element in the past 15 to 250ms...
  // NOTE: periodic 'timeupdate' scheduling is handled by HTMLMediaElement in
  // PlaybackProgressTimerFired().

  // Explicitly cache vector sizes, as their content is constant from here.
  wtf_size_t missed_cues_size = missed_cues.size();
  wtf_size_t previous_cues_size = previous_cues.size();

  // 6 - If all of the cues in current cues have their text track cue active
  // flag set, none of the cues in other cues have their text track cue active
  // flag set, and missed cues is empty, then abort these steps.
  bool active_set_changed = missed_cues_size;

  for (wtf_size_t i = 0; !active_set_changed && i < previous_cues_size; ++i) {
    if (!current_cues.Contains(previous_cues[i]) &&
        previous_cues[i].Data()->IsActive())
      active_set_changed = true;
  }

  for (CueInterval current_cue : current_cues) {
    // Notify any cues that are already active of the current time to mark
    // past and future nodes. Any inactive cues have an empty display state;
    // they will be notified of the current time when the display state is
    // updated.
    if (current_cue.Data()->IsActive())
      current_cue.Data()->UpdatePastAndFutureNodes(movie_time);
    else
      active_set_changed = true;
  }

  if (!active_set_changed)
    return;

  // 7 - If the time was reached through the usual monotonic increase of the
  // current playback position during normal playback, and there are cues in
  // other cues that have their text track cue pause-on-exi flag set and that
  // either have their text track cue active flag set or are also in missed
  // cues, then immediately pause the media element.
  for (wtf_size_t i = 0; !media_element.paused() && i < previous_cues_size;
       ++i) {
    if (previous_cues[i].Data()->pauseOnExit() &&
        previous_cues[i].Data()->IsActive() &&
        !current_cues.Contains(previous_cues[i]))
      media_element.pause();
  }

  for (wtf_size_t i = 0; !media_element.paused() && i < missed_cues_size; ++i) {
    if (missed_cues[i].Data()->pauseOnExit())
      media_element.pause();
  }

  // 8 - Let events be a list of tasks, initially empty. Each task in this
  // list will be associated with a text track, a text track cue, and a time,
  // which are used to sort the list before the tasks are queued.
  HeapVector<std::pair<double, Member<TextTrackCue>>> event_tasks;

  // 8 - Let affected tracks be a list of text tracks, initially empty.
  HeapVector<Member<TextTrack>> affected_tracks;

  for (const auto& missed_cue : missed_cues) {
    // 9 - For each text track cue in missed cues, prepare an event named enter
    // for the TextTrackCue object with the text track cue start time.
    event_tasks.push_back(
        std::make_pair(missed_cue.Data()->startTime(), missed_cue.Data()));

    // 10 - For each text track [...] in missed cues, prepare an event
    // named exit for the TextTrackCue object with the  with the later of
    // the text track cue end time and the text track cue start time.

    // Note: An explicit task is added only if the cue is NOT a zero or
    // negative length cue. Otherwise, the need for an exit event is
    // checked when these tasks are actually queued below. This doesn't
    // affect sorting events before dispatch either, because the exit
    // event has the same time as the enter event.
    if (missed_cue.Data()->startTime() < missed_cue.Data()->endTime()) {
      event_tasks.push_back(
          std::make_pair(missed_cue.Data()->endTime(), missed_cue.Data()));
    }
  }

  for (const auto& previous_cue : previous_cues) {
    // 10 - For each text track cue in other cues that has its text
    // track cue active flag set prepare an event named exit for the
    // TextTrackCue object with the text track cue end time.
    if (!current_cues.Contains(previous_cue)) {
      event_tasks.push_back(
          std::make_pair(previous_cue.Data()->endTime(), previous_cue.Data()));
    }
  }

  for (const auto& current_cue : current_cues) {
    // 11 - For each text track cue in current cues that does not have its
    // text track cue active flag set, prepare an event named enter for the
    // TextTrackCue object with the text track cue start time.
    if (!previous_cues.Contains(current_cue)) {
      event_tasks.push_back(
          std::make_pair(current_cue.Data()->startTime(), current_cue.Data()));
    }
  }

  // 12 - Sort the tasks in events in ascending time order (tasks with earlier
  // times first).
  std::sort(event_tasks.begin(), event_tasks.end(), EventTimeCueCompare);

  for (const auto& task : event_tasks) {
    if (!affected_tracks.Contains(task.second->track()))
      affected_tracks.push_back(task.second->track());

    // 13 - Queue each task in events, in list order.

    // Each event in eventTasks may be either an enterEvent or an exitEvent,
    // depending on the time that is associated with the event. This
    // correctly identifies the type of the event, if the startTime is
    // less than the endTime in the cue.
    if (task.second->startTime() >= task.second->endTime()) {
      media_element.ScheduleEvent(
          CreateEventWithTarget(event_type_names::kEnter, task.second.Get()));
      media_element.ScheduleEvent(
          CreateEventWithTarget(event_type_names::kExit, task.second.Get()));
    } else {
      TextTrackCue* cue = task.second.Get();
      bool is_enter_event = task.first == task.second->startTime();
      AtomicString event_name =
          is_enter_event ? event_type_names::kEnter : event_type_names::kExit;
      media_element.ScheduleEvent(
          CreateEventWithTarget(event_name, task.second.Get()));
      if (features::IsTextBasedAudioDescriptionEnabled()) {
        if (is_enter_event) {
          cue->OnEnter(MediaElement());
        } else {
          cue->OnExit(MediaElement());
        }
      }
    }
  }

  // 14 - Sort affected tracks in the same order as the text tracks appear in
  // the media element's list of text tracks, and remove duplicates.
  std::sort(affected_tracks.begin(), affected_tracks.end(), TrackIndexCompare);

  // 15 - For each text track in affected tracks, in the list order, queue a
  // task to fire a simple event named cuechange at the TextTrack object, and,
  // ...
  for (const auto& track : affected_tracks) {
    media_element.ScheduleEvent(
        CreateEventWithTarget(event_type_names::kCuechange, track.Get()));

    // ... if the text track has a corresponding track element, to then fire a
    // simple event named cuechange at the track element as well.
    if (auto* loadable_text_track = DynamicTo<LoadableTextTrack>(track.Get())) {
      HTMLTrackElement* track_element = loadable_text_track->TrackElement();
      DCHECK(track_element);
      media_element.ScheduleEvent(
          CreateEventWithTarget(event_type_names::kCuechange, track_element));
    }
  }

  // 16 - Set the text track cue active flag of all the cues in the current
  // cues, and unset the text track cue active flag of all the cues in the
  // other cues.
  for (const auto& cue : current_cues)
    cue.Data()->SetIsActive(true);

  for (const auto& previous_cue : previous_cues) {
    if (!current_cues.Contains(previous_cue)) {
      TextTrackCue* cue = previous_cue.Data();
      cue->SetIsActive(false);
      cue->RemoveDisplayTree();
    }
  }

  // Update the current active cues.
  currently_active_cues_ = current_cues;
  media_element.UpdateTextTrackDisplay();
}

void CueTimeline::UpdateActiveCuePastAndFutureNodes() {
  double const movie_time = MediaElement().currentTime();

  for (auto cue : currently_active_cues_) {
    DCHECK(cue.Data()->IsActive());
    if (!cue.Data()->track() || !cue.Data()->track()->IsRendered())
      continue;

    cue.Data()->UpdatePastAndFutureNodes(movie_time);
  }

  SetCueTimestampEventTimer();
}

CueTimeline::IgnoreUpdateScope CueTimeline::BeginIgnoreUpdateScope() {
  DCHECK(!ignore_update_ || !update_requested_while_ignoring_);
  ++ignore_update_;

  IgnoreUpdateScope scope(*this);
  return scope;
}

void CueTimeline::EndIgnoreUpdateScope(base::PassKey<IgnoreUpdateScope>,
                                       IgnoreUpdateScope const& scope) {
  DCHECK(ignore_update_);
  --ignore_update_;

  // If this is the last scope and an update was requested, then perform it
  if (!ignore_update_ && update_requested_while_ignoring_) {
    update_requested_while_ignoring_ = false;
    if (!MediaElement().IsShowPosterFlagSet()) {
      InvokeTimeMarchesOn();
    }
  }
}

void CueTimeline::InvokeTimeMarchesOn() {
  TimeMarchesOn();
  SetCueEventTimer();
  SetCueTimestampEventTimer();
}

void CueTimeline::OnPause() {
  CancelCueEventTimer();
  CancelCueTimestampEventTimer();
}

void CueTimeline::OnPlaybackRateUpdated() {
  SetCueEventTimer();
  SetCueTimestampEventTimer();
}

void CueTimeline::OnReadyStateReset() {
  auto& media_element = MediaElement();
  DCHECK(media_element.getReadyState() == HTMLMediaElement::kHaveNothing);

  // Deactivate all active cues
  // "The user agent must synchronously unset this flag ... whenever the media
  // element's readyState is changed back to HAVE_NOTHING."
  for (auto cue : currently_active_cues_) {
    cue.Data()->SetIsActive(false);
  }
  currently_active_cues_.clear();

  CancelCueEventTimer();
  CancelCueTimestampEventTimer();
  last_update_time_ = -1;

  if (media_element.IsHTMLVideoElement() && media_element.TextTracksVisible()) {
    media_element.UpdateTextTrackDisplay();
  }
}

void CueTimeline::SetCueEventTimer() {
  auto const& media_element = MediaElement();
  if (!next_cue_event_.has_value() || media_element.paused() ||
      media_element.playbackRate() == 0) {
    CancelCueEventTimer();
    return;
  }

  auto const timeout =
      CalculateEventTimeout(next_cue_event_.value(), media_element);
  cue_event_timer_.StartOneShot(timeout, FROM_HERE);
}

void CueTimeline::CancelCueEventTimer() {
  if (cue_event_timer_.IsActive()) {
    cue_event_timer_.Stop();
  }
}

void CueTimeline::CueEventTimerFired(TimerBase*) {
  InvokeTimeMarchesOn();
}

void CueTimeline::CueTimestampEventTimerFired(TimerBase*) {
  UpdateActiveCuePastAndFutureNodes();
  SetCueTimestampEventTimer();
}

void CueTimeline::SetCueTimestampEventTimer() {
  double constexpr kInfinity = std::numeric_limits<double>::infinity();
  auto const& media_element = MediaElement();

  if (media_element.paused() || media_element.playbackRate() == 0) {
    CancelCueTimestampEventTimer();
    return;
  }

  double const movie_time = media_element.currentTime();
  double next_cue_timestamp_event = kInfinity;
  for (auto cue : currently_active_cues_) {
    auto const timestamp = cue.Data()->GetNextIntraCueTime(movie_time);
    next_cue_timestamp_event =
        std::min(next_cue_timestamp_event, timestamp.value_or(kInfinity));
  }

  if (std::isinf(next_cue_timestamp_event)) {
    CancelCueTimestampEventTimer();
    return;
  }

  auto const timeout =
      CalculateEventTimeout(next_cue_timestamp_event, media_element);
  cue_timestamp_event_timer_.StartOneShot(timeout, FROM_HERE);
}

void CueTimeline::CancelCueTimestampEventTimer() {
  if (cue_timestamp_event_timer_.IsActive()) {
    cue_timestamp_event_timer_.Stop();
  }
}

void CueTimeline::DidMoveToNewDocument(Document& /*old_document*/) {
  cue_event_timer_.MoveToNewTaskRunner(
      MediaElement().GetDocument().GetTaskRunner(TaskType::kInternalMedia));
  cue_timestamp_event_timer_.MoveToNewTaskRunner(
      MediaElement().GetDocument().GetTaskRunner(TaskType::kInternalMedia));
}

void CueTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(media_element_);
  visitor->Trace(cue_event_timer_);
  visitor->Trace(cue_timestamp_event_timer_);
}

}  // namespace blink
