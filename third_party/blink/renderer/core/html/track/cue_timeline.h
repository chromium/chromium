// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/pod_interval_tree.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HTMLMediaElement;
class TextTrackCueList;

// TODO(Oilpan): This needs to be PodIntervalTree<double, Member<TextTrackCue>>.
// However, it is not easy to move PodIntervalTree to the heap (for a
// C++-template reason) so we leave it as a raw pointer at the moment. The raw
// pointers are kept alive by `CueTimeline::tree_intervals_`, which mirrors the
// tree contents with traced Members; see the comment there.
using CueIntervalTree = PodIntervalTree<double, TextTrackCue*>;
typedef CueIntervalTree::IntervalType CueInterval;
using CueList = HeapVector<Member<TextTrackCue>>;

// This class manages the timeline and rendering updates of cues associated
// with TextTracks. Owned by a HTMLMediaElement.
class CORE_EXPORT CueTimeline final : public GarbageCollected<CueTimeline> {
 public:
  class [[nodiscard]] IgnoreUpdateScope {
    STACK_ALLOCATED();
    friend CueTimeline;

   public:
    IgnoreUpdateScope(const IgnoreUpdateScope&) = delete;
    IgnoreUpdateScope& operator=(const IgnoreUpdateScope&) = delete;

    ~IgnoreUpdateScope() {
      DCHECK(cue_timeline_);
      cue_timeline_->EndIgnoreUpdateScope(base::PassKey<IgnoreUpdateScope>(),
                                          *this);
    }

   private:
    explicit IgnoreUpdateScope(CueTimeline& cue_timeline)
        : cue_timeline_(&cue_timeline) {}

    CueTimeline* cue_timeline_;
  };

  explicit CueTimeline(HTMLMediaElement&);

  void AddCues(TextTrack*, const TextTrackCueList*);
  void AddCue(TextTrack*, TextTrackCue*);
  void RemoveCues(TextTrack*, const TextTrackCueList*);
  void RemoveCues(TextTrack*, const CueList&);
  void RemoveCue(TextTrack*, TextTrackCue*);
  void WillChangeCue(TextTrack*, TextTrackCue*);
  void HideCues(TextTrack*, const TextTrackCueList*);

  const CueList& CurrentlyActiveCues() const { return currently_active_cues_; }

  // Methods called by |HTMLMediaElement| which affect cue playback.
  void InvokeTimeMarchesOn();
  void OnPause();
  void OnPlaybackRateUpdated();
  void OnReadyStateReset();

  bool InsideIgnoreUpdateScope() const { return ignore_update_ > 0; }
  IgnoreUpdateScope BeginIgnoreUpdateScope();
  void EndIgnoreUpdateScope(base::PassKey<IgnoreUpdateScope>,
                            IgnoreUpdateScope const& scope);

  void DidMoveToNewDocument(Document& old_document);

  void Trace(Visitor*) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CueTimelineTest,
                           CueEventTimerFiredWithPosterFlagSet);

  HTMLMediaElement& MediaElement() const { return *media_element_; }

  void AddCueInternal(TextTrackCue*);
  // Detaches `cue` from the timeline and returns true if it was active.
  bool RemoveCueInternal(TextTrackCue*);
  void TimeMarchesOn();
  void UpdateActiveCuePastAndFutureNodes();

  void CueEventTimerFired(TimerBase*);
  void SetCueEventTimer();
  void CancelCueEventTimer();

  void CueTimestampEventTimerFired(TimerBase*);
  void SetCueTimestampEventTimer();
  void CancelCueTimestampEventTimer();

  Member<HTMLMediaElement> media_element_;

  CueIntervalTree cue_tree_;

  // Mirrors `cue_tree_`: every cue currently in the tree is a key here, mapped
  // to the (start, end) interval it was inserted under. This serves two
  // purposes:
  //  1. The Member keys keep the raw TextTrackCue* stored in `cue_tree_` alive,
  //     so an ordering mistake between a script call-out (e.g. blur/focusout
  //     from VTTCue::RemoveDisplayTree()) and a tree removal cannot leave a
  //     dangling pointer in the tree.
  //  2. RemoveCueInternal() removes the interval that was actually inserted,
  //     rather than one recomputed from the cue's current start/end time.
  using CueTimes = std::pair<double, double>;
  HeapHashMap<Member<TextTrackCue>, CueTimes> tree_intervals_;

  CueList currently_active_cues_;
  HeapHashSet<Member<TextTrackCue>> newly_introduced_cues_;
  double last_update_time_;

  // Timer data for cue events (start, end)
  std::optional<double> next_cue_event_;
  HeapTaskRunnerTimer<CueTimeline> cue_event_timer_;

  // Timer data for cue timestamps
  // https://w3c.github.io/webvtt/#ref-for-webvtt-timestamp-6
  HeapTaskRunnerTimer<CueTimeline> cue_timestamp_event_timer_;

  int ignore_update_;
  bool update_requested_while_ignoring_;
};

#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<TextTrackCue*> {
  static String ToString(TextTrackCue* const& cue) { return cue->ToString(); }
};
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_
