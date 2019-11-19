// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_

#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/pod_interval_tree.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HTMLMediaElement;
class TextTrackCueList;

// TODO(Oilpan): This needs to be PODIntervalTree<double, Member<TextTrackCue>>.
// However, it is not easy to move PODIntervalTree to the heap (for a
// C++-template reason) so we leave it as a raw pointer at the moment. This is
// safe because CueTimeline and TextTrackCue are guaranteed to die at the same
// time when the owner HTMLMediaElement dies. Thus the raw TextTrackCue* cannot
// become stale pointers.
typedef WTF::PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;

// This class manages the timeline and rendering updates of cues associated
// with TextTracks. Owned by a HTMLMediaElement.
class CueTimeline final : public GarbageCollected<CueTimeline> {
 public:
  CueTimeline(HTMLMediaElement&);

  void AddCues(TextTrack*, const TextTrackCueList*);
  void AddCue(TextTrack*, TextTrackCue*);
  void RemoveCues(TextTrack*, const TextTrackCueList*);
  void RemoveCue(TextTrack*, TextTrackCue*);

  void HideCues(TextTrack*, const TextTrackCueList*);

  void UpdateActiveCues(double);

  bool IgnoreUpdateRequests() const { return ignore_update_ > 0; }
  void BeginIgnoringUpdateRequests();
  void EndIgnoringUpdateRequests();

  const CueList& CurrentlyActiveCues() const { return currently_active_cues_; }

  void Trace(Visitor*);

 private:
  HTMLMediaElement& MediaElement() const { return *media_element_; }

  void AddCueInternal(TextTrackCue*);
  void RemoveCueInternal(TextTrackCue*);

  Member<HTMLMediaElement> media_element_;

  CueIntervalTree cue_tree_;

  CueList currently_active_cues_;
  double last_update_time_;

  int ignore_update_;
};

class TrackDisplayUpdateScope {
  STACK_ALLOCATED();

 public:
  TrackDisplayUpdateScope(CueTimeline& cue_timeline) {
    cue_timeline_ = &cue_timeline;
    cue_timeline_->BeginIgnoringUpdateRequests();
  }
  ~TrackDisplayUpdateScope() {
    DCHECK(cue_timeline_);
    cue_timeline_->EndIgnoringUpdateRequests();
  }

 private:
  Member<CueTimeline> cue_timeline_;
};

}  // namespace blink

namespace WTF {
#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<blink::TextTrackCue*> {
  static String ToString(blink::TextTrackCue* const& cue) {
    return cue->ToString();
  }
};
#endif
}

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_CUE_TIMELINE_H_
