// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_TASK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_TASK_CONTROLLER_H_

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalFrame;
class Range;
class TextFinder;
class WebString;
class WebLocalFrameImpl;

class CORE_EXPORT FindTaskController final
    : public GarbageCollected<FindTaskController> {
 public:
  FindTaskController(WebLocalFrameImpl& owner_frame, TextFinder& text_finder);

  // Starts an effort of finding |search_text| in |owner_frame|,
  // which will be done asynchronously with idle tasks.
  void StartRequest(int identifier,
                    const WebString& search_text,
                    const mojom::blink::FindOptions& options);

  // Cancels the current effort, canceling the idle task request
  // and resets variables related to the current state.
  void CancelPendingRequest();

  LocalFrame* GetLocalFrame() const;

  WebLocalFrameImpl& OwnerFrame() const {
    DCHECK(owner_frame_);
    return *owner_frame_;
  }

  // Determines whether the finding effort is required for a particular frame.
  // It is not necessary if the frame is invisible, for example, or if this
  // is a repeat search that already returned nothing last time the same prefix
  // was searched.
  bool ShouldFindMatches(const String& search_text,
                         const mojom::blink::FindOptions& options);

  // During a run of |idle_find_task|, we found a match.
  // Updates |current_match_count_| and notifies |text_finder_|.
  void DidFindMatch(int identifier, Range* result_range);

  // One run of idle task finishes, so we need to update our state and
  // notify |text_finder_| accordingly. Also schedules next task if needed.
  void DidFinishTask(int identifier,
                     const WebString& search_text,
                     const mojom::blink::FindOptions& options,
                     bool finished_whole_request,
                     PositionInFlatTree next_starting_position,
                     int match_count);

  Range* ResumeFindingFromRange() const { return resume_finding_from_range_; }
  int CurrentMatchCount() const { return current_match_count_; }

  // Idle task, when invoked will search for a given text and notify us
  // whenever a match is found or when it finishes, through FoundMatch and
  // DidFinishTask.
  class IdleFindTask;

  void Trace(Visitor* visitor);

  void ResetLastFindRequestCompletedWithNoMatches();

 private:
  void RequestIdleFindTask(int identifier,
                           const WebString& search_text,
                           const mojom::blink::FindOptions& options);

  Member<WebLocalFrameImpl> owner_frame_;

  Member<TextFinder> text_finder_;

  Member<IdleFindTask> idle_find_task_;

  // Keeps track if there is any ongoing find effort or not.
  bool finding_in_progress_;

  // Keeps track of how many matches the current finding effort has
  // found so far.
  int current_match_count_;

  // The finding effort can time out and we need to keep track of where we
  // ended our last search so we can continue from where we left of.
  //
  // This range is collapsed to the end position of the last successful
  // search; the new search should start from this position.
  Member<Range> resume_finding_from_range_;

  // Keeps track of whether the last find request completed its finding effort
  // without finding any matches in this frame.
  bool last_find_request_completed_with_no_matches_;

  // Keeps track of the last string this frame searched for. This is used for
  // short-circuiting searches in the following scenarios: When a frame has
  // been searched and returned 0 results, we don't need to search that frame
  // again if the user is just adding to the search (making it more specific).
  WTF::String last_search_string_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_TASK_CONTROLLER_H_
