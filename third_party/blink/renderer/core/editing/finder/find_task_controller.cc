// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_task_controller.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/idle_request_options.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/iterators/search_buffer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {
const int kFindingTimeoutMS = 100;
constexpr TimeDelta kFindTaskTestTimeout = TimeDelta::FromSeconds(10);
}  // namespace

class FindTaskController::IdleFindTask
    : public ScriptedIdleTaskController::IdleTask {
 public:
  static IdleFindTask* Create(FindTaskController* controller,
                              Document* document,
                              int identifier,
                              const WebString& search_text,
                              const mojom::blink::FindOptions& options) {
    return new IdleFindTask(controller, document, identifier, search_text,
                            options);
  }

  void Dispose() {
    DCHECK_GT(callback_handle_, 0);
    document_->CancelIdleCallback(callback_handle_);
  }

  void ForceInvocationForTesting() {
    invoke(IdleDeadline::Create(CurrentTimeTicks() + kFindTaskTestTimeout,
                                IdleDeadline::CallbackType::kCalledWhenIdle));
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    visitor->Trace(document_);
    ScriptedIdleTaskController::IdleTask::Trace(visitor);
  }

 private:
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
    IdleRequestOptions request_options;
    request_options.setTimeout(kFindingTimeoutMS);
    callback_handle_ = document_->RequestIdleCallback(this, request_options);
  }

  void invoke(IdleDeadline* deadline) override {
    if (!controller_->ShouldFindMatches(search_text_, *options_)) {
      controller_->DidFinishTask(identifier_, search_text_, *options_,
                                 true /* finished_whole_request */,
                                 PositionInFlatTree(), 0 /* match_count */);
    }

    const TimeDelta time_available =
        TimeDelta::FromMillisecondsD(deadline->timeRemaining());
    const TimeTicks start_time = CurrentTimeTicks();

    PositionInFlatTree search_start = PositionInFlatTree::FirstPositionInNode(
        *controller_->GetLocalFrame()->GetDocument());
    PositionInFlatTree search_end = PositionInFlatTree::LastPositionInNode(
        *controller_->GetLocalFrame()->GetDocument());
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

    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  see http://crbug.com/590369 for more details.
    search_start.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

    int match_count = 0;
    bool full_range_searched = false;
    PositionInFlatTree next_scoping_start;
    do {
      // Find next occurrence of the search string.
      // FIXME: (http://crbug.com/6818) This WebKit operation may run for longer
      // than the timeout value, and is not interruptible as it is currently
      // written. We may need to rewrite it with interruptibility in mind, or
      // find an alternative.
      const EphemeralRangeInFlatTree result = FindPlainText(
          EphemeralRangeInFlatTree(search_start, search_end), search_text_,
          options_->match_case ? 0 : kCaseInsensitive);
      if (result.IsCollapsed()) {
        // Not found.
        full_range_searched = true;
        break;
      }
      Range* result_range = Range::Create(
          result.GetDocument(), ToPositionInDOMTree(result.StartPosition()),
          ToPositionInDOMTree(result.EndPosition()));
      if (result_range->collapsed()) {
        // resultRange will be collapsed if the matched text spans over multiple
        // TreeScopes.  FIXME: Show such matches to users.
        search_start = result.EndPosition();
        if (deadline->timeRemaining() > 0)
          continue;
        break;
      }
      ++match_count;
      controller_->DidFindMatch(identifier_, result_range);

      // Set the new start for the search range to be the end of the previous
      // result range. There is no need to use a VisiblePosition here,
      // since findPlainText will use a TextIterator to go over the visible
      // text nodes.
      search_start = result.EndPosition();
      next_scoping_start = search_start;
    } while (deadline->timeRemaining() > 0);

    const TimeDelta time_spent = CurrentTimeTicks() - start_time;
    UMA_HISTOGRAM_TIMES("WebCore.FindInPage.ScopingTime",
                        time_spent - time_available);

    controller_->DidFinishTask(identifier_, search_text_, *options_,
                               full_range_searched, next_scoping_start,
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
  idle_find_task_ = IdleFindTask::Create(this, GetLocalFrame()->GetDocument(),
                                         identifier, search_text, options);
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
    resume_finding_from_range_ =
        Range::Create(*next_starting_position.GetDocument(),
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

}  // namespace blink
