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

#include "third_party/blink/renderer/core/script/html_parser_script_runner.h"

#include <inttypes.h>
#include <memory>
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/html/parser/html_input_stream.h"
#include "third_party/blink/renderer/core/script/html_parser_script_runner_host.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {

// TODO(bmcquade): move this to a shared location if we find ourselves wanting
// to trace similar data elsewhere in the codebase.
std::unique_ptr<TracedValue> GetTraceArgsForScriptElement(
    Document& document,
    const TextPosition& text_position,
    const KURL& url) {
  auto value = std::make_unique<TracedValue>();
  if (!url.IsNull())
    value->SetString("url", url.GetString());
  if (document.GetFrame()) {
    value->SetString(
        "frame",
        String::Format("0x%" PRIx64,
                       static_cast<uint64_t>(
                           reinterpret_cast<intptr_t>(document.GetFrame()))));
  }
  if (text_position.line_.ZeroBasedInt() > 0 ||
      text_position.column_.ZeroBasedInt() > 0) {
    value->SetInteger("lineNumber", text_position.line_.OneBasedInt());
    value->SetInteger("columnNumber", text_position.column_.OneBasedInt());
  }
  return value;
}

void DoExecuteScript(PendingScript* pending_script, Document& document) {
  TRACE_EVENT_WITH_FLOW1(
      "blink", "HTMLParserScriptRunner ExecuteScript",
      pending_script->GetElement(), TRACE_EVENT_FLAG_FLOW_IN, "data",
      GetTraceArgsForScriptElement(document, pending_script->StartingPosition(),
                                   pending_script->UrlForTracing()));
  pending_script->ExecuteScriptBlock();
}

void TraceParserBlockingScript(const PendingScript* pending_script,
                               Document& document) {
  // The HTML parser must yield before executing script in the following
  // cases:
  // * the script's execution is blocked on the completed load of the script
  //   resource
  //   (https://html.spec.whatwg.org/C/#pending-parsing-blocking-script)
  // * the script's execution is blocked on the load of a style sheet or other
  //   resources that are blocking scripts
  //   (https://html.spec.whatwg.org/C/#a-style-sheet-that-is-blocking-scripts)
  //
  // Both of these cases can introduce significant latency when loading a
  // web page, especially for users on slow connections, since the HTML parser
  // must yield until the blocking resources finish loading.
  //
  // We trace these parser yields here using flow events, so we can track
  // both when these yields occur, as well as how long the parser had
  // to yield. The connecting flow events are traced once the parser becomes
  // unblocked when the script actually executes, in doExecuteScript.
  ScriptElementBase* element = pending_script->GetElement();
  if (!element)
    return;
  bool waiting_for_resources = !document.IsScriptExecutionReady();

  auto script_element_trace_lambda = [&]() {
    return GetTraceArgsForScriptElement(document,
                                        pending_script->StartingPosition(),
                                        pending_script->UrlForTracing());
  };
  if (!pending_script->IsReady()) {
    if (waiting_for_resources) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoadAndBlockingResources", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data", script_element_trace_lambda());
    } else {
      TRACE_EVENT_WITH_FLOW1("blink", "YieldParserForScriptLoad", element,
                             TRACE_EVENT_FLAG_FLOW_OUT, "data",
                             script_element_trace_lambda());
    }
  } else if (waiting_for_resources) {
    TRACE_EVENT_WITH_FLOW1("blink", "YieldParserForScriptBlockingResources",
                           element, TRACE_EVENT_FLAG_FLOW_OUT, "data",
                           script_element_trace_lambda());
  }
}

}  // namespace

HTMLParserScriptRunner::HTMLParserScriptRunner(
    HTMLParserReentryPermit* reentry_permit,
    Document* document,
    HTMLParserScriptRunnerHost* host)
    : reentry_permit_(reentry_permit), document_(document), host_(host) {
  DCHECK(host_);
}

HTMLParserScriptRunner::~HTMLParserScriptRunner() {}

void HTMLParserScriptRunner::Detach() {
  if (!document_)
    return;

  if (parser_blocking_script_)
    parser_blocking_script_->Dispose();
  parser_blocking_script_ = nullptr;

  while (!scripts_to_execute_after_parsing_.empty()) {
    PendingScript* pending_script =
        scripts_to_execute_after_parsing_.TakeFirst();
    pending_script->Dispose();
  }
  document_ = nullptr;
  // m_reentryPermit is not cleared here, because the script runner
  // may continue to run pending scripts after the parser has
  // detached.
}

bool HTMLParserScriptRunner::IsParserBlockingScriptReady() {
  DCHECK(ParserBlockingScript());
  if (!document_->IsScriptExecutionReady())
    return false;
  // TODO(crbug.com/1344772) Consider moving this condition to
  // Document::IsScriptExecutionReady(), while we are not yet sure.
  if (base::FeatureList::IsEnabled(features::kForceInOrderScript) &&
      document_->GetScriptRunner()->HasForceInOrderScripts())
    return false;
  return ParserBlockingScript()->IsReady();
}

// Corresponds to some steps of the "Otherwise" Clause of 'An end tag whose
// tag name is "script"'
// <specdef href="https://html.spec.whatwg.org/C/#scriptEndTag">
void HTMLParserScriptRunner::
    ExecutePendingParserBlockingScriptAndDispatchEvent() {
  // <spec step="B.1">Let the script be the pending parsing-blocking
  // script.</spec>
  PendingScript* pending_script = parser_blocking_script_;

  // Stop watching loads before executeScript to prevent recursion if the script
  // reloads itself.
  // TODO(kouhei): Consider merging this w/ pendingScript->dispose() after the
  // if block.
  // TODO(kouhei, hiroshige): Consider merging this w/ the code clearing
  // |parser_blocking_script_| below.
  pending_script->StopWatchingForLoad();

  if (!IsExecutingScript()) {
    // TODO(kouhei, hiroshige): Investigate why we need checkpoint here.
    document_->GetAgent().event_loop()->PerformMicrotaskCheckpoint();
    // The parser cannot be unblocked as a microtask requested another
    // resource
    if (!document_->IsScriptExecutionReady())
      return;
  }

  // <spec step="B.2">Set the pending parsing-blocking script to null.</spec>
  parser_blocking_script_ = nullptr;

  {
    // <spec step="B.10">Increment the parser's script nesting level by one (it
    // should be zero before this step, so this sets it to one).</spec>
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nesting_level_incrementer =
            reentry_permit_->IncrementScriptNestingLevel();

    // <spec step="B.11">Execute the script element the script.</spec>
    DCHECK(IsExecutingScript());
    DoExecuteScript(pending_script, *document_);

    // <spec step="B.12">Decrement the parser's script nesting level by one. If
    // the parser's script nesting level is zero (which it always should be at
    // this point), then set the parser pause flag to false.</spec>
    //
    // This is implemented by ~ScriptNestingLevelIncrementer().
  }

  DCHECK(!IsExecutingScript());
}

// Should be correspond to
//
// <specdef
// href="https://html.spec.whatwg.org/C/#execute-the-script-element">
//
// but currently does more than specced, because historically this and
// ExecutePendingParserBlockingScriptAndDispatchEvent() was the same method.
void HTMLParserScriptRunner::ExecutePendingDeferredScriptAndDispatchEvent(
    PendingScript* pending_script) {
  // Stop watching loads before executeScript to prevent recursion if the script
  // reloads itself.
  // TODO(kouhei): Consider merging this w/ pendingScript->dispose() after the
  // if block.
  pending_script->StopWatchingForLoad();

  if (!IsExecutingScript()) {
    // TODO(kouhei, hiroshige): Investigate why we need checkpoint here.
    document_->GetAgent().event_loop()->PerformMicrotaskCheckpoint();
  }

  DoExecuteScript(pending_script, *document_);
}

void HTMLParserScriptRunner::PendingScriptFinished(
    PendingScript* pending_script) {
  // Handle cancellations of parser-blocking script loads without
  // notifying the host (i.e., parser) if these were initiated by nested
  // document.write()s. The cancellation may have been triggered by
  // script execution to signal an abrupt stop (e.g., window.close().)
  //
  // The parser is unprepared to be told, and doesn't need to be.
  if (IsExecutingScript() && pending_script->WasCanceled()) {
    pending_script->Dispose();

    DCHECK_EQ(pending_script, ParserBlockingScript());
    parser_blocking_script_ = nullptr;

    return;
  }

  // Posting the script execution part to a new task so that we can allow
  // yielding for cooperative scheduling. Cooperative scheduling requires that
  // the Blink C++ stack be thin when it executes JavaScript.
  document_->GetTaskRunner(TaskType::kInternalContinueScriptLoading)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&HTMLParserScriptRunnerHost::NotifyScriptLoaded,
                               WrapPersistent(host_.Get())));
}

// <specdef href="https://html.spec.whatwg.org/C/#scriptEndTag">
//
// Script handling lives outside the tree builder to keep each class simple.
void HTMLParserScriptRunner::ProcessScriptElement(
    Element* script_element,
    const TextPosition& script_start_position) {
  DCHECK(script_element);

  // FIXME: If scripting is disabled, always just return.

  bool had_preload_scanner = host_->HasPreloadScanner();

  // <spec>An end tag whose tag name is "script" ...</spec>
  //
  // Try to execute the script given to us.
  ProcessScriptElementInternal(script_element, script_start_position);

  // <spec>... At this stage, if the pending parsing-blocking script is not
  // null, then:</spec>
  if (HasParserBlockingScript()) {
    if (IsExecutingScript()) {
      // <spec step="A">If the script nesting level is not zero:
      //
      // Set the parser pause flag to true, and abort the processing of any
      // nested invocations of the tokenizer, yielding control back to the
      // caller. (Tokenization will resume when the caller returns to the
      // "outer" tree construction stage.)</spec>

      // Unwind to the outermost HTMLParserScriptRunner::processScriptElement
      // before continuing parsing.
      return;
    }

    // - "Otherwise":

    TraceParserBlockingScript(ParserBlockingScript(), *document_);
    parser_blocking_script_->MarkParserBlockingLoadStartTime();

    // If preload scanner got created, it is missing the source after the
    // current insertion point. Append it and scan.
    if (!had_preload_scanner && host_->HasPreloadScanner())
      host_->AppendCurrentInputStreamToPreloadScannerAndScan();

    ExecuteParsingBlockingScripts();
  }
}

bool HTMLParserScriptRunner::HasParserBlockingScript() const {
  return ParserBlockingScript();
}

// <specdef href="https://html.spec.whatwg.org/C/#scriptEndTag">
//
// <spec>An end tag whose tag name is "script" ...</spec>
void HTMLParserScriptRunner::ExecuteParsingBlockingScripts() {
  // <spec step="B">Otherwise:
  //
  // While the pending parsing-blocking script is not null:</spec>
  //
  // <spec step="B.5">If the parser's Document has a style sheet that is
  // blocking scripts or the script's ready to be parser-executed is false: spin
  // the event loop until the parser's Document has no style sheet that is
  // blocking scripts and the script's ready to be parser-executed becomes
  // true.</spec>
  //
  // These conditions correspond to IsParserBlockingScriptReady().
  // If it is false at the time of #prepare-the-script-element,
  // ExecuteParsingBlockingScripts() will be called later
  // when IsParserBlockingScriptReady() might become true:
  // - Called from HTMLParserScriptRunner::ExecuteScriptsWaitingForResources()
  //   when the parser's Document has no style sheet that is blocking scripts,
  // - Called from HTMLParserScriptRunner::ExecuteScriptsWaitingForLoad()
  //   when the script's "ready to be parser-executed" flag is set, or
  // - Other cases where any of the conditions isn't met or even when there are
  //   no longer parser blocking scripts at all.
  //   (For example, see the comment in ExecuteScriptsWaitingForLoad())
  //
  // Because we check the conditions below and do nothing if the conditions
  // aren't met, it's safe to have extra ExecuteParsingBlockingScripts() calls.
  while (HasParserBlockingScript() && IsParserBlockingScriptReady()) {
    DCHECK(document_);
    DCHECK(!IsExecutingScript());
    DCHECK(document_->IsScriptExecutionReady());

    // <spec step="B.9">Let the insertion point be just before the next input
    // character.</spec>
    InsertionPointRecord insertion_point_record(host_->InputStream());

    ExecutePendingParserBlockingScriptAndDispatchEvent();

    // <spec step="B.13">Let the insertion point be undefined again.</spec>
    //
    // Implemented as ~InsertionPointRecord().
  }
}

void HTMLParserScriptRunner::ExecuteScriptsWaitingForLoad() {
  // Note(https://crbug.com/1093051): ExecuteScriptsWaitingForLoad() is
  // triggered asynchronously from PendingScriptFinished(pending_script), but
  // the |pending_script| might be no longer the ParserBlockginScript() here,
  // because it might have been evaluated or disposed after
  // PendingScriptFinished() before ExecuteScriptsWaitingForLoad(). Anyway we
  // call ExecuteParsingBlockingScripts(), because necessary conditions for
  // evaluation are checked safely there.

  TRACE_EVENT0("blink", "HTMLParserScriptRunner::executeScriptsWaitingForLoad");
  DCHECK(!IsExecutingScript());
  ExecuteParsingBlockingScripts();
}

void HTMLParserScriptRunner::ExecuteScriptsWaitingForResources() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForResources");
  DCHECK(document_);
  DCHECK(!IsExecutingScript());
  DCHECK(document_->IsScriptExecutionReady());
  ExecuteParsingBlockingScripts();
}

// <specdef href="https://html.spec.whatwg.org/C/#stop-parsing">
PendingScript* HTMLParserScriptRunner::TryTakeReadyScriptWaitingForParsing(
    HeapDeque<Member<PendingScript>>* waiting_scripts) {
  DCHECK(!waiting_scripts->empty());

  // <spec step="5.1">Spin the event loop until the first script in the list of
  // scripts that will execute when the document has finished parsing has its
  // ready to be parser-executed set to true and the parser's Document has no
  // style sheet that is blocking scripts.</spec>
  if (!document_->IsScriptExecutionReady())
    return nullptr;
  PendingScript* script = waiting_scripts->front();
  if (!script->IsReady()) {
    if (!script->IsWatchingForLoad()) {
      // First time when all the conditions except for
      // `PendingScript::IsReady()` are satisfied. Note that
      // `TryTakeReadyScriptWaitingForParsing()` can triggered by script and
      // stylesheet load completions multiple times, so `IsWatchingForLoad()` is
      // checked to avoid double execution of this code block. When
      // `IsWatchingForLoad()` is true, its existing client is always `this`.
      script->WatchForLoad(this);
      TraceParserBlockingScript(script, *document_);
      script->MarkParserBlockingLoadStartTime();
    }
    return nullptr;
  }
  return waiting_scripts->TakeFirst().Get();
}

// <specdef href="https://html.spec.whatwg.org/C/#stop-parsing">
//
// This will run the developer deferred scripts.
bool HTMLParserScriptRunner::ExecuteScriptsWaitingForParsing() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForParsing");

  // <spec step="5">While the list of scripts that will execute when the
  // document has finished parsing is not empty:</spec>
  while (!scripts_to_execute_after_parsing_.empty()) {
    DCHECK(!IsExecutingScript());
    DCHECK(!HasParserBlockingScript());
    DCHECK(scripts_to_execute_after_parsing_.front()->IsExternalOrModule());

    // <spec step="5.3">Remove the first script element from the list of scripts
    // that will execute when the document has finished parsing (i.e. shift out
    // the first entry in the list).</spec>
    PendingScript* first =
        TryTakeReadyScriptWaitingForParsing(&scripts_to_execute_after_parsing_);
    if (!first)
      return false;

    // <spec step="5.2">Execute the script element given by the first script in
    // the list of scripts that will execute when the document has finished
    // parsing.</spec>
    ExecutePendingDeferredScriptAndDispatchEvent(first);

    // FIXME: What is this m_document check for?
    if (!document_)
      return false;
  }

  return true;
}

// The initial steps for 'An end tag whose tag name is "script"'
// <specdef href="https://html.spec.whatwg.org/C/#scriptEndTag">
// <specdef label="prepare-the-script-element"
// href="https://html.spec.whatwg.org/C/#prepare-the-script-element">
void HTMLParserScriptRunner::ProcessScriptElementInternal(
    Element* script,
    const TextPosition& script_start_position) {
  DCHECK(document_);
  DCHECK(!HasParserBlockingScript());
  {
    ScriptLoader* script_loader = ScriptLoaderFromElement(script);

    // FIXME: Align trace event name and function name.
    TRACE_EVENT1("blink", "HTMLParserScriptRunner::execute", "data",
                 GetTraceArgsForScriptElement(*document_, script_start_position,
                                              NullURL()));
    DCHECK(script_loader->IsParserInserted());

    // <spec>... If the active speculative HTML parser is null and the
    // JavaScript execution context stack is empty, then perform a microtask
    // checkpoint. ...</spec>
    if (!IsExecutingScript())
      document_->GetAgent().event_loop()->PerformMicrotaskCheckpoint();

    // <spec>... Let the old insertion point have the same value as the current
    // insertion point. Let the insertion point be just before the next input
    // character. ...</spec>
    InsertionPointRecord insertion_point_record(host_->InputStream());

    // <spec>... Increment the parser's script nesting level by one. ...</spec>
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nesting_level_incrementer =
            reentry_permit_->IncrementScriptNestingLevel();

    // <spec>... prepare the script element script. This might cause some script
    // to execute, which might cause new characters to be inserted into the
    // tokenizer, and might cause the tokenizer to output more tokens, resulting
    // in a reentrant invocation of the parser. ...</spec>
    PendingScript* pending_script = script_loader->PrepareScript(
        reentry_permit_->ScriptNestingLevel() == 1u
            ? ScriptLoader::ParserBlockingInlineOption::kAllow
            : ScriptLoader::ParserBlockingInlineOption::kDeny,
        script_start_position);

    if (!pending_script)
      return;

    switch (pending_script->GetSchedulingType()) {
      case ScriptSchedulingType::kDefer:
        // Developer deferred.
        DCHECK(pending_script->IsExternalOrModule());
        // <spec
        // href="https://html.spec.whatwg.org/C/#prepare-the-script-element"
        // step="31.4.1">Append el to its parser document's list of scripts that
        // will execute when the document has finished parsing.</spec>
        scripts_to_execute_after_parsing_.push_back(pending_script);
        break;

      case ScriptSchedulingType::kParserBlocking:
        // <spec label="prepare-the-script-element" step="31.5.1">Set el's
        // parser document's pending parsing-blocking script to el.</spec>
      case ScriptSchedulingType::kParserBlockingInline:
        // <spec label="prepare-the-script-element" step="32.2.1">Set el's
        // parser document's pending parsing-blocking script to el.</spec>

        CHECK(!parser_blocking_script_);
        parser_blocking_script_ = pending_script;

        // We only care about a load callback if resource is not yet ready.
        // The caller of `ProcessScriptElementInternal()` will attempt to run
        // `parser_blocking_script_` if ready before returning control to the
        // parser.
        if (!parser_blocking_script_->IsReady())
          parser_blocking_script_->WatchForLoad(this);
        break;

      case ScriptSchedulingType::kAsync:
      case ScriptSchedulingType::kInOrder:
      case ScriptSchedulingType::kForceInOrder:
      case ScriptSchedulingType::kImmediate:
      case ScriptSchedulingType::kNotSet:
      case ScriptSchedulingType::kDeprecatedForceDefer:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    // <spec>... Decrement the parser's script nesting level by one. If the
    // parser's script nesting level is zero, then set the parser pause flag to
    // false. ...</spec>
    //
    // Implemented by ~ScriptNestingLevelIncrementer().

    // <spec>... Let the insertion point have the value of the old insertion
    // point. ...</spec>
    //
    // Implemented by ~InsertionPointRecord().
  }
}

void HTMLParserScriptRunner::Trace(Visitor* visitor) const {
  visitor->Trace(reentry_permit_);
  visitor->Trace(document_);
  visitor->Trace(host_);
  visitor->Trace(parser_blocking_script_);
  visitor->Trace(scripts_to_execute_after_parsing_);
  PendingScriptClient::Trace(visitor);
}

}  // namespace blink
