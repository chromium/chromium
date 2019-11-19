// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_task_controller.h"

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/idle_request_options.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

namespace {
const int kFindingTimeoutMS = 100;
constexpr base::TimeDelta kFindTaskTestTimeout =
    base::TimeDelta::FromSeconds(10);
}  // namespace

class FindTaskController::IdleFindTask
    : public ScriptedIdleTaskController::IdleTask {
 public:
  IdleFindTask(FindTaskController* controller,
               Document* document,
               int identifier,
               const WebString& search_text,
               const mojom::blink::FindOptions& options)
      : document_(document),
        controller_(controller),
        identifier_(identifier),
        search_text_(search_text),
        options_(options.Clone()) {
    DCHECK(document_);
    // We need to add deadline because some webpages might have frames
    // that are always busy, resulting in bad experience in find-in-page
    // because the scoping tasks are not run.
    // See crbug.com/893465.
    IdleRequestOptions* request_options = IdleRequestOptions::Create();
    request_options->setTimeout(kFindingTimeoutMS);
    callback_handle_ = document_->RequestIdleCallback(this, request_options);
  }

  void Dispose() {
    DCHECK_GT(callback_handle_, 0);
    document_->CancelIdleCallback(callback_handle_);
  }

  void ForceInvocationForTesting() {
    invoke(MakeGarbageCollected<IdleDeadline>(
        base::TimeTicks::Now() + kFindTaskTestTimeout,
        IdleDeadline::CallbackType::kCalledWhenIdle));
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    visitor->Trace(document_);
    ScriptedIdleTaskController::IdleTask::Trace(visitor);
  }

 private:
  void invoke(IdleDeadline* deadline) override {
    if (!controller_->ShouldFindMatches(search_text_, *options_)) {
      controller_->DidFinishTask(identifier_, search_text_, *options_,
                                 true /* finished_whole_request */,
                                 PositionInFlatTree(), 0 /* match_count */);
    }

    Document& document = *controller_->GetLocalFrame()->GetDocument();
    PositionInFlatTree search_start =
        PositionInFlatTree::FirstPositionInNode(document);
    PositionInFlatTree search_end;
    if (document.documentElement() && document.documentElement()->lastChild()) {
      search_end = PositionInFlatTree::AfterNode(
          *document.documentElement()->lastChild());
    } else {
      search_end = PositionInFlatTree::LastPositionInNode(document);
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

    // TODO(editing-dev): Use of UpdateStyleAndLayout
    // needs to be audited.  see http://crbug.com/590369 for more details.
    search_start.GetDocument()->UpdateStyleAndLayout();

    int match_count = 0;
    bool full_range_searched = false;
    PositionInFlatTree next_task_start_position;

    blink::FindOptions find_options =
        (options_->forward ? 0 : kBackwards) |
        (options_->match_case ? 0 : kCaseInsensitive) |
        (options_->find_next ? 0 : kStartInSelection);

    while (search_start != search_end) {
      // Find in the whole block.
      FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end));
      std::unique_ptr<FindBuffer::Results> match_results =
          buffer.FindMatches(search_text_, find_options);
      for (FindBuffer::BufferMatchResult match : *match_results) {
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
      }
      // At this point, all text in the block collected above has been
      // processed. Now we move to the next block if there's any,
      // otherwise we should stop.
      search_start = buffer.PositionAfterBlock();
      if (search_start.IsNull()) {
        full_range_searched = true;
        break;
      }
      next_task_start_position = search_start;
      if (deadline->timeRemaining() <= 0)
        break;
    }

    controller_->DidFinishTask(identifier_, search_text_, *options_,
                               full_range_searched, next_task_start_position,
                               match_count);
  }

  Member<Document> document_;
  Member<FindTaskController> controller_;
  int callback_handle_ = 0;
  const int identifier_;
  const WebString search_text_;
  mojom::blink::FindOptionsPtr options_;
};

FindTaskController::FindTaskController(WebLocalFrameImpl& owner_frame,
                                       TextFinder& text_finder)
    : owner_frame_(owner_frame),
      text_finder_(text_finder),
      resume_finding_from_range_(nullptr) {}

void FindTaskController::StartRequest(
    int identifier,
    const WebString& search_text,
    const mojom::blink::FindOptions& options) {
  // This is a brand new search, so we need to reset everything.
  finding_in_progress_ = true;
  current_match_count_ = 0;
  RequestIdleFindTask(identifier, search_text, options);
}

void FindTaskController::CancelPendingRequest() {
  if (idle_find_task_) {
    idle_find_task_->Dispose();
    idle_find_task_.Clear();
  }
  if (finding_in_progress_)
    last_find_request_completed_with_no_matches_ = false;
  finding_in_progress_ = false;
  resume_finding_from_range_ = nullptr;
}

void FindTaskController::RequestIdleFindTask(
    int identifier,
    const WebString& search_text,
    const mojom::blink::FindOptions& options) {
  DCHECK_EQ(idle_find_task_, nullptr);
  idle_find_task_ = MakeGarbageCollected<IdleFindTask>(
      this, GetLocalFrame()->GetDocument(), identifier, search_text, options);
  // If it's for testing, run the task immediately.
  // TODO(rakina): Change to use general solution when it's available.
  // https://crbug.com/875203
  if (options.run_synchronously_for_testing)
    idle_find_task_->ForceInvocationForTesting();
}

void FindTaskController::DidFinishTask(
    int identifier,
    const WebString& search_text,
    const mojom::blink::FindOptions& options,
    bool finished_whole_request,
    PositionInFlatTree next_starting_position,
    int match_count) {
  if (idle_find_task_) {
    idle_find_task_->Dispose();
    idle_find_task_.Clear();
  }
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
    // Idle task ran out of time, request for another one.
    RequestIdleFindTask(identifier, search_text, options);
    return;  // Done for now, resume work later.
  }

  text_finder_->FinishCurrentScopingEffort(identifier);

  last_find_request_completed_with_no_matches_ = !current_match_count_;
  finding_in_progress_ = false;
}

LocalFrame* FindTaskController::GetLocalFrame() const {
  return OwnerFrame().GetFrame();
}

bool FindTaskController::ShouldFindMatches(
    const String& search_text,
    const mojom::blink::FindOptions& options) {
  // Don't scope if we can't find a frame or a view.
  // The user may have closed the tab/application, so abort.
  LocalFrame* frame = GetLocalFrame();
  if (!frame || !frame->View() || !frame->GetPage())
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
      !last_search_string_.IsEmpty()) {
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

void FindTaskController::Trace(Visitor* visitor) {
  visitor->Trace(owner_frame_);
  visitor->Trace(text_finder_);
  visitor->Trace(idle_find_task_);
  visitor->Trace(resume_finding_from_range_);
}

void FindTaskController::ResetLastFindRequestCompletedWithNoMatches() {
  last_find_request_completed_with_no_matches_ = false;
}

}  // namespace blink
