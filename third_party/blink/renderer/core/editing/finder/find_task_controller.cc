// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_task_controller.h"

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/finder/find_results.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

namespace {
constexpr base::TimeDelta kFindTaskTimeAllotment = base::Milliseconds(10);

// Check if we need to yield after this many matches have been found. We start
// with Start matches and double them every time we yield until we are
// processing Limit matches per yield check.
constexpr int kMatchYieldCheckIntervalStart = 100;
constexpr int kMatchYieldCheckIntervalLimit = 6400;

}  // namespace

class FindTaskController::FindTask final : public GarbageCollected<FindTask> {
 public:
  FindTask(FindTaskController* controller,
           Document* document,
           int identifier,
           const String& search_text,
           const mojom::blink::FindOptions& options)
      : document_(document),
        controller_(controller),
        identifier_(identifier),
        search_text_(search_text),
        options_(options.Clone()) {
    DCHECK(document_);
    if (options.run_synchronously_for_testing) {
      Invoke();
    } else {
      controller_->GetLocalFrame()
          ->GetTaskRunner(blink::TaskType::kInternalFindInPage)
          ->PostTask(FROM_HERE, WTF::BindOnce(&FindTask::Invoke,
                                              WrapWeakPersistent(this)));
    }
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(controller_);
    visitor->Trace(document_);
  }

  void Invoke() {
    const base::TimeTicks task_start_time = base::TimeTicks::Now();
    if (!controller_)
      return;
    if (!controller_->ShouldFindMatches(identifier_, search_text_, *options_)) {
      controller_->DidFinishTask(identifier_, search_text_, *options_,
                                 true /* finished_whole_request */,
                                 PositionInFlatTree(), 0 /* match_count */,
                                 true /* aborted */, task_start_time);
      return;
    }
    SCOPED_UMA_HISTOGRAM_TIMER("WebCore.FindInPage.TaskDuration");

    Document* document = controller_->GetLocalFrame()->GetDocument();
    if (!document || document_ != document)
      return;
    auto forced_activatable_display_locks =
        document->GetDisplayLockDocumentState()
            .GetScopedForceActivatableLocks();
    PositionInFlatTree search_start =
        PositionInFlatTree::FirstPositionInNode(*document);
    PositionInFlatTree search_end;
    if (document->documentElement() &&
        document->documentElement()->lastChild()) {
      search_end = PositionInFlatTree::AfterNode(
          *document->documentElement()->lastChild());
    } else {
      search_end = PositionInFlatTree::LastPositionInNode(*document);
    }
    DCHECK_EQ(search_start.GetDocument(), search_end.GetDocument());

    if (Range* resume_from_range = controller_->ResumeFindingFromRange()) {
      // This is a continuation of a finding operation that timed out and didn't
      // complete last time around, so we should start from where we left off.
      DCHECK(resume_from_range->collapsed());
      search_start = FromPositionInDOMTree<EditingInFlatTreeStrategy>(
          resume_from_range->EndPosition());
      if (search_start.GetDocument() != search_end.GetDocument())
        return;
    }

    // This is required if we forced any of the display-locks.
    document->UpdateStyleAndLayout(DocumentUpdateReason::kFindInPage);

    int match_count = 0;
    bool full_range_searched = true;
    PositionInFlatTree next_task_start_position;

    auto find_options = FindOptions()
                            .SetBackwards(!options_->forward)
                            .SetCaseInsensitive(!options_->match_case)
                            .SetStartingInSelection(options_->new_session);
    const auto start_time = base::TimeTicks::Now();

    auto time_allotment_expired = [start_time]() {
      auto time_elapsed = base::TimeTicks::Now() - start_time;
      return time_elapsed > kFindTaskTimeAllotment;
    };

    int match_yield_check_interval = controller_->GetMatchYieldCheckInterval();

    while (search_start < search_end) {
      // Find in the whole block.
      FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end),
                        RubySupport::kEnabledIfNecessary);
      FindResults match_results =
          buffer.FindMatches(search_text_, find_options);
      bool yielded_while_iterating_results = false;
      for (MatchResultICU match : match_results) {
        const EphemeralRangeInFlatTree ephemeral_match_range =
            buffer.RangeFromBufferIndex(match.start,
                                        match.start + match.length);
        auto* const match_range = MakeGarbageCollected<Range>(
            ephemeral_match_range.GetDocument(),
            ToPositionInDOMTree(ephemeral_match_range.StartPosition()),
            ToPositionInDOMTree(ephemeral_match_range.EndPosition()));
        if (match_range->collapsed()) {
          // resultRange will be collapsed if the matched text spans over
          // multiple TreeScopes.  TODO(rakina): Show such matches to users.
          next_task_start_position = ephemeral_match_range.EndPosition();
          continue;
        }
        ++match_count;
        controller_->DidFindMatch(identifier_, match_range);

        // Check if we should yield. Since we accumulate text on block
        // boundaries, if a lot of the text is in a single block, then we may
        // get stuck in there processing all of the matches. It's not so bad per
        // se, but when coupled with updating painting of said matches and the
        // scrollbar ticks, then we can block the main thread for quite some
        // time.
        if ((match_count % match_yield_check_interval) == 0 &&
            time_allotment_expired()) {
          // Next time we should start at the end of the current match.
          next_task_start_position = ephemeral_match_range.EndPosition();
          yielded_while_iterating_results = true;
          break;
        }
      }

      // If we have yielded from the inner loop, then just break out of the
      // loop, since we already updated the next_task_start_position.
      if (yielded_while_iterating_results) {
        full_range_searched = false;
        break;
      }

      // At this point, all text in the block collected above has been
      // processed. Now we move to the next block if there's any,
      // otherwise we should stop.
      search_start = buffer.PositionAfterBlock();
      if (search_start.IsNull() || search_start >= search_end) {
        full_range_searched = true;
        break;
      }

      // We should also check if we should yield after every block search, since
      // it's a nice natural boundary. Note that if we yielded out of the inner
      // loop, then we should exit before updating the search_start position to
      // the PositionAfterBlock. Otherwise, we may miss the matches that happen
      // in the same block. This block updates next_task_start_position to be
      // the updated search_start.
      if (time_allotment_expired()) {
        next_task_start_position = search_start;
        full_range_searched = false;
        break;
      }
    }
    controller_->DidFinishTask(identifier_, search_text_, *options_,
                               full_range_searched, next_task_start_position,
                               match_count, false /* aborted */,
                               task_start_time);
  }

  Member<Document> document_;
  Member<FindTaskController> controller_;
  const int identifier_;
  const String search_text_;
  mojom::blink::FindOptionsPtr options_;
};

FindTaskController::FindTaskController(WebLocalFrameImpl& owner_frame,
                                       TextFinder& text_finder)
    : owner_frame_(owner_frame),
      text_finder_(text_finder),
      resume_finding_from_range_(nullptr),
      match_yield_check_interval_(kMatchYieldCheckIntervalStart) {}

int FindTaskController::GetMatchYieldCheckInterval() const {
  return match_yield_check_interval_;
}

void FindTaskController::StartRequest(
    int identifier,
    const String& search_text,
    const mojom::blink::FindOptions& options) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "blink", "FindInPageRequest",
      TRACE_ID_WITH_SCOPE("FindInPageRequest", identifier));
  DCHECK(!finding_in_progress_);
  DCHECK_EQ(current_find_identifier_, kInvalidFindIdentifier);
  // This is a brand new search, so we need to reset everything.
  finding_in_progress_ = true;
  current_match_count_ = 0;
  current_find_identifier_ = identifier;
  match_yield_check_interval_ = kMatchYieldCheckIntervalStart;
  RequestFindTask(identifier, search_text, options);
}

void FindTaskController::CancelPendingRequest() {
  if (find_task_)
    find_task_.Clear();
  if (finding_in_progress_) {
    last_find_request_completed_with_no_matches_ = false;
  }
  finding_in_progress_ = false;
  resume_finding_from_range_ = nullptr;
  current_find_identifier_ = kInvalidFindIdentifier;
}

void FindTaskController::RequestFindTask(
    int identifier,
    const String& search_text,
    const mojom::blink::FindOptions& options) {
  DCHECK_EQ(find_task_, nullptr);
  DCHECK_EQ(identifier, current_find_identifier_);
  find_task_ = MakeGarbageCollected<FindTask>(
      this, GetLocalFrame()->GetDocument(), identifier, search_text, options);
}

void FindTaskController::DidFinishTask(
    int identifier,
    const String& search_text,
    const mojom::blink::FindOptions& options,
    bool finished_whole_request,
    PositionInFlatTree next_starting_position,
    int match_count,
    bool aborted,
    base::TimeTicks task_start_time) {
  if (current_find_identifier_ != identifier)
    return;
  if (find_task_)
    find_task_.Clear();
  // Remember what we search for last time, so we can skip searching if more
  // letters are added to the search string (and last outcome was 0).
  last_search_string_ = search_text;

  if (next_starting_position.IsNotNull()) {
    resume_finding_from_range_ = MakeGarbageCollected<Range>(
        *next_starting_position.GetDocument(),
        ToPositionInDOMTree(next_starting_position),
        ToPositionInDOMTree(next_starting_position));
  }

  if (match_count > 0) {
    text_finder_->UpdateMatches(identifier, match_count,
                                finished_whole_request);
  }

  if (!finished_whole_request) {
    match_yield_check_interval_ = std::min(kMatchYieldCheckIntervalLimit,
                                           2 * match_yield_check_interval_);
    // Task ran out of time, request for another one.
    RequestFindTask(identifier, search_text, options);
    return;  // Done for now, resume work later.
  }

  text_finder_->FinishCurrentScopingEffort(identifier);

  last_find_request_completed_with_no_matches_ =
      !aborted && !current_match_count_;
  finding_in_progress_ = false;
  current_find_identifier_ = kInvalidFindIdentifier;
}

LocalFrame* FindTaskController::GetLocalFrame() const {
  return OwnerFrame().GetFrame();
}

bool FindTaskController::ShouldFindMatches(
    int identifier,
    const String& search_text,
    const mojom::blink::FindOptions& options) {
  if (identifier != current_find_identifier_)
    return false;
  // Don't scope if we can't find a frame, a document, or a view.
  // The user may have closed the tab/application, so abort.
  LocalFrame* frame = GetLocalFrame();
  if (!frame || !frame->View() || !frame->GetPage() || !frame->GetDocument())
    return false;

  DCHECK(frame->GetDocument());
  DCHECK(frame->View());

  if (options.force)
    return true;

  if (!OwnerFrame().HasVisibleContent())
    return false;

  // If the frame completed the scoping operation and found 0 matches the last
  // time it was searched, then we don't have to search it again if the user is
  // just adding to the search string or sending the same search string again.
  if (last_find_request_completed_with_no_matches_ &&
      !last_search_string_.empty()) {
    // Check to see if the search string prefixes match.
    String previous_search_prefix =
        search_text.Substring(0, last_search_string_.length());

    if (previous_search_prefix == last_search_string_)
      return false;  // Don't search this frame, it will be fruitless.
  }

  return true;
}

void FindTaskController::DidFindMatch(int identifier, Range* result_range) {
  current_match_count_++;
  text_finder_->DidFindMatch(identifier, current_match_count_, result_range);
}

void FindTaskController::Trace(Visitor* visitor) const {
  visitor->Trace(owner_frame_);
  visitor->Trace(text_finder_);
  visitor->Trace(find_task_);
  visitor->Trace(resume_finding_from_range_);
}

void FindTaskController::ResetLastFindRequestCompletedWithNoMatches() {
  last_find_request_completed_with_no_matches_ = false;
}

}  // namespace blink
