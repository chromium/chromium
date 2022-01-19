/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_SCHEDULER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/core/html/parser/nesting_level_incrementer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLDocumentParser;

class SpeculationsPumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  explicit SpeculationsPumpSession(unsigned& nesting_level);
  ~SpeculationsPumpSession();

  base::TimeDelta ElapsedTime() const { return start_time_.Elapsed(); }
  void AddedElementTokens(wtf_size_t count);
  wtf_size_t ProcessedElementTokens() const {
    return processed_element_tokens_;
  }

 private:
  base::ElapsedTimer start_time_;
  wtf_size_t processed_element_tokens_;
};

class HTMLParserScheduler final : public GarbageCollected<HTMLParserScheduler> {
 public:
  HTMLParserScheduler(HTMLDocumentParser*,
                      scoped_refptr<base::SingleThreadTaskRunner>);
  HTMLParserScheduler(const HTMLParserScheduler&) = delete;
  HTMLParserScheduler& operator=(const HTMLParserScheduler&) = delete;
  ~HTMLParserScheduler();

  bool IsScheduledForUnpause() const;
  void ScheduleForUnpause();
  bool YieldIfNeeded(const SpeculationsPumpSession&, bool starting_script);

  void Detach();  // Clear active tasks if any.

  void Trace(Visitor*) const;

 private:
  bool ShouldYield(const SpeculationsPumpSession&, bool starting_script) const;
  void ContinueParsing();

  Member<HTMLDocumentParser> parser_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  TaskHandle cancellable_continue_parse_task_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_SCHEDULER_H_
