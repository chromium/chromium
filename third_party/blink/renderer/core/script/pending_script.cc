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

#include "third_party/blink/renderer/core/script/pending_script.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/ignore_destructive_write_count_incrementer.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {

namespace {
WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
    ScriptElementBase* element) {
  if (!element || !element->GetDocument().GetFrame())
    return WebScopedVirtualTimePauser();
  return element->GetDocument()
      .GetFrame()
      ->GetFrameScheduler()
      ->CreateWebScopedVirtualTimePauser(
          "PendingScript",
          WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
}
}  // namespace

// See the comment about |is_in_document_write| in ScriptLoader::PrepareScript()
// about IsInDocumentWrite() use here.
PendingScript::PendingScript(ScriptElementBase* element,
                             const TextPosition& starting_position)
    : element_(element),
      starting_position_(starting_position),
      virtual_time_pauser_(CreateWebScopedVirtualTimePauser(element)),
      client_(nullptr),
      original_element_document_(&element->GetDocument()),
      original_context_document_(element->GetDocument().ContextDocument()),
      created_during_document_write_(
          element->GetDocument().IsInDocumentWrite()) {}

PendingScript::~PendingScript() {}

void PendingScript::Dispose() {
  StopWatchingForLoad();
  DCHECK(!Client());
  DCHECK(!IsWatchingForLoad());

  starting_position_ = TextPosition::BelowRangePosition();
  parser_blocking_load_start_time_ = base::TimeTicks();

  DisposeInternal();
  element_ = nullptr;
}

void PendingScript::WatchForLoad(PendingScriptClient* client) {
  CheckState();

  DCHECK(!IsWatchingForLoad());
  DCHECK(client);
  // addClient() will call streamingFinished() if the load is complete. Callers
  // who do not expect to be re-entered from this call should not call
  // watchForLoad for a PendingScript which isReady. We also need to set
  // m_watchingForLoad early, since addClient() can result in calling
  // notifyFinished and further stopWatchingForLoad().
  client_ = client;
  if (IsReady()) {
    PendingScriptFinished();
  } else {
    virtual_time_pauser_.PauseVirtualTime();
  }
}

void PendingScript::StopWatchingForLoad() {
  if (!IsWatchingForLoad())
    return;
  CheckState();
  DCHECK(IsExternalOrModule());
  client_ = nullptr;
  virtual_time_pauser_.UnpauseVirtualTime();
}

void PendingScript::PendingScriptFinished() {
  virtual_time_pauser_.UnpauseVirtualTime();
  if (client_) {
    client_->PendingScriptFinished(this);
  }
}

ScriptElementBase* PendingScript::GetElement() const {
  // As mentioned in the comment at |m_element| declaration,
  // |m_element|  must point to the corresponding ScriptLoader's
  // client.
  CHECK(element_);
  return element_.Get();
}

void PendingScript::MarkParserBlockingLoadStartTime() {
  DCHECK(parser_blocking_load_start_time_.is_null());
  parser_blocking_load_start_time_ = base::TimeTicks::Now();
}

// <specdef href="https://html.spec.whatwg.org/C/#execute-the-script-block">
void PendingScript::ExecuteScriptBlock(const KURL& document_url) {
  TRACE_EVENT0("blink", "PendingScript::ExecuteScriptBlock");
  Document* context_document = element_->GetDocument().ContextDocument();
  if (!context_document) {
    Dispose();
    return;
  }

  LocalFrame* frame = context_document->GetFrame();
  if (!frame) {
    Dispose();
    return;
  }

  if (OriginalContextDocument() != context_document) {
    // Do not execute scripts if they are moved between context documents.
    Dispose();
    return;
  }

  if (original_element_document_ != &element_->GetDocument()) {
    // Do not execute scripts if they are moved between element documents (under
    // the same context Document).

    // We continue counting for a while to confirm that such cases are really
    // rare on stable channel. https://crbug.com/721914
    UseCounter::Count(context_document,
                      WebFeature::kEvaluateScriptMovedBetweenElementDocuments);

    Dispose();
    return;
  }

  Script* script = GetSource(document_url);

  const bool was_canceled = WasCanceled();
  const bool is_external = IsExternal();
  const bool created_during_document_write = WasCreatedDuringDocumentWrite();
  const base::TimeTicks parser_blocking_load_start_time =
      ParserBlockingLoadStartTime();
  const bool is_controlled_by_script_runner = IsControlledByScriptRunner();
  ScriptElementBase* element = element_;
  Dispose();

  // ExecuteScriptBlockInternal() is split just in order to prevent accidential
  // access to |this| after Dispose().
  ExecuteScriptBlockInternal(
      script, element, was_canceled, is_external, created_during_document_write,
      parser_blocking_load_start_time, is_controlled_by_script_runner);
}

// <specdef href="https://html.spec.whatwg.org/C/#execute-the-script-block">
void PendingScript::ExecuteScriptBlockInternal(
    Script* script,
    ScriptElementBase* element,
    bool was_canceled,
    bool is_external,
    bool created_during_document_write,
    base::TimeTicks parser_blocking_load_start_time,
    bool is_controlled_by_script_runner) {
  Document& element_document = element->GetDocument();
  Document* context_document = element_document.ContextDocument();

  // <spec step="2">If the script's script is null, fire an event named error at
  // the element, and return.</spec>
  if (!script) {
    element->DispatchErrorEvent();
    return;
  }

  if (parser_blocking_load_start_time > base::TimeTicks()) {
    DocumentParserTiming::From(element_document)
        .RecordParserBlockedOnScriptLoadDuration(
            base::TimeTicks::Now() - parser_blocking_load_start_time,
            created_during_document_write);
  }

  if (was_canceled)
    return;

  base::TimeTicks script_exec_start_time = base::TimeTicks::Now();

  {
    if (element->ElementHasDuplicateAttributes()) {
      UseCounter::Count(element_document,
                        WebFeature::kDuplicatedAttributeForExecutedScript);
    }

    const bool is_imported_script = context_document != &element_document;

    // <spec step="3">If the script is from an external file, or the script's
    // type is "module", ...</spec>
    const bool needs_increment =
        is_external || script->GetScriptType() == mojom::ScriptType::kModule ||
        is_imported_script;
    // <spec step="3">... then increment the ignore-destructive-writes counter
    // of the script element's node document. Let neutralized doc be that
    // Document.</spec>
    IgnoreDestructiveWriteCountIncrementer incrementer(
        needs_increment ? context_document : nullptr);

    // <spec step="4.A.1">Let old script element be the value to which the
    // script element's node document's currentScript object was most recently
    // set.</spec>
    //
    // This is implemented as push/popCurrentScript().

    // <spec step="4">Switch on the script's type:</spec>

    // <spec step="4.A">"classic"</spec>
    //
    // <spec step="4.A.2">If the script element's root is not a shadow root,
    // then set the script element's node document's currentScript attribute to
    // the script element. Otherwise, set it to null.</spec>
    //
    // Note: The shadow root check is implemented in
    // HTMLScriptElement::SetScriptElementForBinding().

    // <spec step="4.B">"module"</spec>
    //
    // <spec step="4.B.1">Assert: The script element's node document's
    // currentScript attribute is null.</spec>
    ScriptElementBase* current_script = nullptr;
    if (script->GetScriptType() == mojom::ScriptType::kClassic)
      current_script = element;
    context_document->PushCurrentScript(current_script);

    // <spec step="4.A">"classic"</spec>
    //
    // <spec step="4.A.3">Run the classic script given by the script's
    // script.</spec>
    //
    // Note: This is where the script is compiled and actually executed.
    //
    // <spec step="4.B">"module"</spec>
    //
    // <spec step="4.B.2">Run the module script given by the script's
    // script.</spec>
    script->RunScript(context_document->GetFrame(),
                      element_document.GetSecurityOrigin());

    // <spec step="4.A.4">Set the script element's node document's currentScript
    // attribute to old script element.</spec>
    context_document->PopCurrentScript(current_script);

    // <spec step="5">Decrement the ignore-destructive-writes counter of
    // neutralized doc, if it was incremented in the earlier step.</spec>
    //
    // Implemented as the scope out of IgnoreDestructiveWriteCountIncrementer.
  }

  // NOTE: we do not check m_willBeParserExecuted here, since
  // m_willBeParserExecuted is false for inline scripts, and we want to
  // include inline script execution time as part of parser blocked script
  // execution time.
  if (!is_controlled_by_script_runner) {
    DocumentParserTiming::From(element_document)
        .RecordParserBlockedOnScriptExecutionDuration(
            base::TimeTicks::Now() - script_exec_start_time,
            created_during_document_write);
  }

  // <spec step="6">If the script is from an external file, then fire an event
  // named load at the script element.</spec>
  if (is_external)
    element->DispatchLoadEvent();
}

void PendingScript::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(client_);
  visitor->Trace(original_context_document_);
  visitor->Trace(original_element_document_);
}

bool PendingScript::IsControlledByScriptRunner() const {
  switch (scheduling_type_) {
    case ScriptSchedulingType::kNotSet:
      NOTREACHED();
      return false;

    case ScriptSchedulingType::kDefer:
    case ScriptSchedulingType::kParserBlocking:
    case ScriptSchedulingType::kParserBlockingInline:
    case ScriptSchedulingType::kImmediate:
    case ScriptSchedulingType::kForceDefer:
      return false;

    case ScriptSchedulingType::kInOrder:
    case ScriptSchedulingType::kAsync:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace blink
