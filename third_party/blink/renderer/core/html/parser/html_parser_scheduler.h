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

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/core/html/parser/nesting_level_incrementer.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLDocumentParser;

class SpeculationsPumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  SpeculationsPumpSession(unsigned& nesting_level);
  ~SpeculationsPumpSession();

  base::TimeDelta ElapsedTime() const;
  void AddedElementTokens(size_t count);
  size_t ProcessedElementTokens() const { return processed_element_tokens_; }

 private:
  base::ElapsedTimer start_time_;
  size_t processed_element_tokens_;
};

class HTMLParserScheduler final : public GarbageCollected<HTMLParserScheduler> {
 public:
  HTMLParserScheduler(HTMLDocumentParser*,
                      scoped_refptr<base::SingleThreadTaskRunner>);
  ~HTMLParserScheduler();

  bool IsScheduledForUnpause() const;
  void ScheduleForUnpause();
  bool YieldIfNeeded(const SpeculationsPumpSession&, bool starting_script);

  /**
   * Can only be called if this scheduler is paused. If this is called,
   * then after the scheduler is resumed by calling resume(), this call
   * ensures that HTMLDocumentParser::resumeAfterYield will be called. Used to
   * signal this scheduler that the background html parser sent chunks to
   * HTMLDocumentParser while it was paused.
   */
  void ForceUnpauseAfterYield();

  void Pause();
  void Unpause();

  void Detach();  // Clear active tasks if any.

  void Trace(Visitor*);

 private:
  bool ShouldYield(const SpeculationsPumpSession&, bool starting_script) const;
  void ContinueParsing();

  Member<HTMLDocumentParser> parser_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  TaskHandle cancellable_continue_parse_task_handle_;
  bool is_paused_with_active_timer_;

  DISALLOW_COPY_AND_ASSIGN(HTMLParserScheduler);
};

}  // namespace blink

#endif
