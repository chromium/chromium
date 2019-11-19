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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/spellcheck/text_checking.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class SpellCheckRequester;
class WebTextCheckClient;

class CORE_EXPORT SpellCheckRequest final
    : public GarbageCollected<SpellCheckRequest> {
 public:
  static const int kUnrequestedTextCheckingSequence = -1;

  static SpellCheckRequest* Create(const EphemeralRange& checking_range,
                                   int request_number);

  SpellCheckRequest(Range* checking_range, const String&, int request_number);
  ~SpellCheckRequest();
  void Dispose();

  Range* CheckingRange() const { return checking_range_; }
  Element* RootEditableElement() const { return root_editable_element_; }

  void SetCheckerAndSequence(SpellCheckRequester*, int sequence);
  int Sequence() const { return sequence_; }
  String GetText() const { return text_; }

  bool IsValid() const;
  void DidSucceed(const Vector<TextCheckingResult>&);
  void DidCancel();

  int RequestNumber() const { return request_number_; }

  void Trace(Visitor*);

 private:
  Member<SpellCheckRequester> requester_;
  Member<Range> checking_range_;
  Member<Element> root_editable_element_;
  int sequence_ = kUnrequestedTextCheckingSequence;
  String text_;
  int request_number_;
};

class CORE_EXPORT SpellCheckRequester final
    : public GarbageCollected<SpellCheckRequester> {
 public:
  explicit SpellCheckRequester(LocalFrame&);
  ~SpellCheckRequester();
  void Trace(Visitor*);

  // Returns true if a request is initiated. Returns false otherwise.
  bool RequestCheckingFor(const EphemeralRange&);
  bool RequestCheckingFor(const EphemeralRange&, int request_num);
  void CancelCheck();

  int LastRequestSequence() const { return last_request_sequence_; }

  int LastProcessedSequence() const { return last_processed_sequence_; }

  // Called to clean up pending requests when no more checking is needed. For
  // example, when document is closed.
  void Deactivate();

 private:
  friend class SpellCheckRequest;

  WebTextCheckClient* GetTextCheckerClient() const;
  void TimerFiredToProcessQueuedRequest(TimerBase*);
  void InvokeRequest(SpellCheckRequest*);
  void EnqueueRequest(SpellCheckRequest*);
  bool EnsureValidRequestQueueFor(int sequence);
  void DidCheckSucceed(int sequence, const Vector<TextCheckingResult>&);
  void DidCheckCancel(int sequence);
  void DidCheck(int sequence);

  void ClearProcessingRequest();

  Member<LocalFrame> frame_;
  LocalFrame& GetFrame() const {
    DCHECK(frame_);
    return *frame_;
  }

  int last_request_sequence_;
  int last_processed_sequence_;
  base::TimeTicks last_request_time_;

  TaskRunnerTimer<SpellCheckRequester> timer_to_process_queued_request_;

  Member<SpellCheckRequest> processing_request_;

  typedef HeapDeque<Member<SpellCheckRequest>> RequestQueue;
  RequestQueue request_queue_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckRequester);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SPELLCHECK_SPELL_CHECK_REQUESTER_H_
