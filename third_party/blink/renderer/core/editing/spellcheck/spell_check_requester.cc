/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

namespace {

static Vector<TextCheckingResult> ToCoreResults(
    const WebVector<WebTextCheckingResult>& results) {
  Vector<TextCheckingResult> core_results;
  for (size_t i = 0; i < results.size(); ++i)
    core_results.push_back(results[i]);
  return core_results;
}

class WebTextCheckingCompletionImpl : public WebTextCheckingCompletion {
 public:
  explicit WebTextCheckingCompletionImpl(SpellCheckRequest* request)
      : request_(request) {}

  void DidFinishCheckingText(
      const WebVector<WebTextCheckingResult>& results) override {
    if (request_)
      request_->DidSucceed(ToCoreResults(results));
    request_ = nullptr;
  }

  void DidCancelCheckingText() override {
    if (request_)
      request_->DidCancel();
    request_ = nullptr;
  }

  ~WebTextCheckingCompletionImpl() override = default;

 private:
  // As |WebTextCheckingCompletionImpl| is mananaged outside Blink, it should
  // only keep weak references to Blink objects to prevent memory leaks.
  WeakPersistent<SpellCheckRequest> request_;
};

}  // namespace

SpellCheckRequest::SpellCheckRequest(Range* checking_range,
                                     const String& text,
                                     int request_number)
    : requester_(nullptr),
      checking_range_(checking_range),
      root_editable_element_(
          blink::RootEditableElement(*checking_range_->startContainer())),
      text_(text),
      request_number_(request_number) {
  DCHECK(checking_range_);
  DCHECK(checking_range_->IsConnected());
}

SpellCheckRequest::~SpellCheckRequest() = default;

void SpellCheckRequest::Trace(Visitor* visitor) {
  visitor->Trace(requester_);
  visitor->Trace(checking_range_);
  visitor->Trace(root_editable_element_);
}

void SpellCheckRequest::Dispose() {
  if (checking_range_)
    checking_range_->Dispose();
}

// static
SpellCheckRequest* SpellCheckRequest::Create(
    const EphemeralRange& checking_range,
    int request_number) {
  if (checking_range.IsNull())
    return nullptr;
  if (!blink::RootEditableElement(
          *checking_range.StartPosition().ComputeContainerNode()))
    return nullptr;

  String text =
      PlainText(checking_range, TextIteratorBehavior::Builder()
                                    .SetEmitsObjectReplacementCharacter(true)
                                    .Build());
  if (text.IsEmpty())
    return nullptr;

  Range* checking_range_object = CreateRange(checking_range);

  SpellCheckRequest* request = MakeGarbageCollected<SpellCheckRequest>(
      checking_range_object, text, request_number);
  if (request->RootEditableElement())
    return request;

  // We may reach here if |checking_range| crosses shadow boundary, in which
  // case we don't want spellchecker to crash renderer.
  request->Dispose();
  return nullptr;
}

bool SpellCheckRequest::IsValid() const {
  return checking_range_->IsConnected() &&
         root_editable_element_->isConnected();
}

void SpellCheckRequest::DidSucceed(const Vector<TextCheckingResult>& results) {
  if (!requester_)
    return;
  SpellCheckRequester* requester = requester_;
  requester_ = nullptr;
  requester->DidCheckSucceed(sequence_, results);
}

void SpellCheckRequest::DidCancel() {
  if (!requester_)
    return;
  SpellCheckRequester* requester = requester_;
  requester_ = nullptr;
  requester->DidCheckCancel(sequence_);
}

void SpellCheckRequest::SetCheckerAndSequence(SpellCheckRequester* requester,
                                              int sequence) {
  DCHECK(!requester_);
  DCHECK_EQ(sequence_, kUnrequestedTextCheckingSequence);
  requester_ = requester;
  sequence_ = sequence;
}

SpellCheckRequester::SpellCheckRequester(LocalFrame& frame)
    : frame_(&frame),
      last_request_sequence_(0),
      last_processed_sequence_(0),
      timer_to_process_queued_request_(
          frame.GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SpellCheckRequester::TimerFiredToProcessQueuedRequest) {}

SpellCheckRequester::~SpellCheckRequester() = default;

WebTextCheckClient* SpellCheckRequester::GetTextCheckerClient() const {
  return GetFrame().GetSpellChecker().GetTextCheckerClient();
}

void SpellCheckRequester::TimerFiredToProcessQueuedRequest(TimerBase*) {
  DCHECK(!request_queue_.IsEmpty());
  if (request_queue_.IsEmpty())
    return;

  InvokeRequest(request_queue_.TakeFirst());
}

bool SpellCheckRequester::RequestCheckingFor(const EphemeralRange& range) {
  return RequestCheckingFor(range, 0);
}

bool SpellCheckRequester::RequestCheckingFor(const EphemeralRange& range,
                                             int request_num) {
  SpellCheckRequest* request = SpellCheckRequest::Create(range, request_num);
  if (!request)
    return false;

  const base::TimeTicks current_request_time = base::TimeTicks::Now();
  if (request_num == 0 && last_request_time_ > base::TimeTicks()) {
    UMA_HISTOGRAM_TIMES("WebCore.SpellChecker.RequestInterval",
                        current_request_time - last_request_time_);
  }
  last_request_time_ = current_request_time;

  DCHECK_EQ(request->Sequence(),
            SpellCheckRequest::kUnrequestedTextCheckingSequence);
  int sequence = ++last_request_sequence_;
  if (sequence == SpellCheckRequest::kUnrequestedTextCheckingSequence)
    sequence = ++last_request_sequence_;

  request->SetCheckerAndSequence(this, sequence);

  if (timer_to_process_queued_request_.IsActive() || processing_request_)
    EnqueueRequest(request);
  else
    InvokeRequest(request);

  return true;
}

void SpellCheckRequester::CancelCheck() {
  if (processing_request_)
    processing_request_->DidCancel();
}

void SpellCheckRequester::Deactivate() {
  timer_to_process_queued_request_.Stop();
  // Empty all pending requests to prevent them from being a leak source, as the
  // requests may hold reference to a closed document.
  request_queue_.clear();
  // Must be called after clearing the queue. Otherwise, another request from
  // the queue will be invoked.
  CancelCheck();
}

void SpellCheckRequester::InvokeRequest(SpellCheckRequest* request) {
  DCHECK(!processing_request_);
  processing_request_ = request;
  if (WebTextCheckClient* text_checker_client = GetTextCheckerClient()) {
    text_checker_client->RequestCheckingOfText(
        processing_request_->GetText(),
        std::make_unique<WebTextCheckingCompletionImpl>(request));
  }
}

void SpellCheckRequester::ClearProcessingRequest() {
  if (!processing_request_)
    return;

  processing_request_->Dispose();
  processing_request_.Clear();
}

void SpellCheckRequester::EnqueueRequest(SpellCheckRequest* request) {
  DCHECK(request);
  bool continuation = false;
  if (!request_queue_.IsEmpty()) {
    SpellCheckRequest* last_request = request_queue_.back();
    // It's a continuation if the number of the last request got incremented in
    // the new one and both apply to the same editable.
    continuation =
        request->RootEditableElement() == last_request->RootEditableElement() &&
        request->RequestNumber() == last_request->RequestNumber() + 1;
  }

  // Spellcheck requests for chunks of text in the same element should not
  // overwrite each other.
  if (!continuation) {
    RequestQueue::const_iterator same_element_request = std::find_if(
        request_queue_.begin(), request_queue_.end(),
        [request](const SpellCheckRequest* queued_request) -> bool {
          return request->RootEditableElement() ==
                 queued_request->RootEditableElement();
        });
    if (same_element_request != request_queue_.end())
      request_queue_.erase(same_element_request);
  }

  request_queue_.push_back(request);
}

bool SpellCheckRequester::EnsureValidRequestQueueFor(int sequence) {
  DCHECK(processing_request_);
  if (processing_request_->Sequence() == sequence)
    return true;
  NOTREACHED();
  request_queue_.clear();
  return false;
}

void SpellCheckRequester::DidCheck(int sequence) {
  DCHECK_LT(last_processed_sequence_, sequence);
  last_processed_sequence_ = sequence;

  ClearProcessingRequest();
  if (!request_queue_.IsEmpty())
    timer_to_process_queued_request_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void SpellCheckRequester::DidCheckSucceed(
    int sequence,
    const Vector<TextCheckingResult>& results) {
  if (!EnsureValidRequestQueueFor(sequence))
    return;
  GetFrame().GetSpellChecker().MarkAndReplaceFor(processing_request_, results);
  DidCheck(sequence);
}

void SpellCheckRequester::DidCheckCancel(int sequence) {
  if (!EnsureValidRequestQueueFor(sequence))
    return;
  DidCheck(sequence);
}

void SpellCheckRequester::Trace(Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(processing_request_);
  visitor->Trace(request_queue_);
}

}  // namespace blink
