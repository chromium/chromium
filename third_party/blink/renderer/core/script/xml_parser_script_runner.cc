// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/xml_parser_script_runner.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/xml_parser_script_runner_host.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Spec links:
// <specdef label="Parsing"
// href="https://html.spec.whatwg.org/C/#parsing-xhtml-documents">
// <specdef label="Prepare"
// href="https://html.spec.whatwg.org/C/#prepare-the-script-element">

XMLParserScriptRunner::XMLParserScriptRunner(XMLParserScriptRunnerHost* host)
    : host_(host) {}

XMLParserScriptRunner::~XMLParserScriptRunner() {
  DCHECK(!parser_blocking_script_);
}

void XMLParserScriptRunner::Trace(Visitor* visitor) const {
  visitor->Trace(parser_blocking_script_);
  visitor->Trace(host_);
  PendingScriptClient::Trace(visitor);
}

void XMLParserScriptRunner::Detach() {
  if (parser_blocking_script_) {
    parser_blocking_script_->StopWatchingForLoad();
    parser_blocking_script_ = nullptr;
  }
}

void XMLParserScriptRunner::PendingScriptFinished(
    PendingScript* unused_pending_script) {
  DCHECK_EQ(unused_pending_script, parser_blocking_script_);
  PendingScript* pending_script = parser_blocking_script_;
  parser_blocking_script_ = nullptr;

  pending_script->StopWatchingForLoad();

  CHECK_EQ(pending_script->GetScriptType(), mojom::blink::ScriptType::kClassic);

  // <spec label="Parsing" step="4">Execute the script element given by the
  // pending parsing-blocking script.</spec>
  pending_script->ExecuteScriptBlock();

  // <spec label="Parsing" step="5">Set the pending parsing-blocking script to
  // null.</spec>
  DCHECK(!parser_blocking_script_);

  // <spec label="Parsing" step="3">Unblock this instance of the XML parser,
  // such that tasks that invoke it can again be run.</spec>
  host_->NotifyScriptExecuted();
}

void XMLParserScriptRunner::ProcessScriptElement(
    Document& document,
    Element* element,
    TextPosition script_start_position) {
  DCHECK(element);
  DCHECK(!parser_blocking_script_);

  // [Parsing] When the element's end tag is subsequently parsed, the user agent
  // must perform a microtask checkpoint, and then prepare the script element.
  // [spec text]
  PendingScript* pending_script =
      ScriptLoaderFromElement(element)->PrepareScript(
          ScriptLoader::ParserBlockingInlineOption::kAllow,
          script_start_position);

  if (!pending_script)
    return;

  if (pending_script->GetScriptType() == mojom::blink::ScriptType::kModule) {
    // XMLDocumentParser does not support defer scripts, and thus ignores all
    // module scripts.
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kError,
        "Module scripts in XML documents are currently "
        "not supported. See crbug.com/717643"));
    return;
  }

  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kParserBlockingInline:
      // <spec label="Prepare" step="31.4.2">... (The parser will handle
      // executing the script.)</spec>
      //
      // <spec label="Parsing" step="4">Execute the script element given by the
      // pending parsing-blocking script.</spec>
      //
      // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
      // is blocking scripts and thus the script is executed immediately here,
      // and thus Steps 1-3 are skipped.
      pending_script->ExecuteScriptBlock();
      break;

    case ScriptSchedulingType::kDefer:
      // XMLParserScriptRunner doesn't support defer scripts and handle them as
      // if parser-blocking scripts.
    case ScriptSchedulingType::kParserBlocking:
      // <spec label="Prepare" step="31.5.1">Set el's parser document's pending
      // parsing-blocking script to el.</spec>
      parser_blocking_script_ = pending_script;
      parser_blocking_script_->MarkParserBlockingLoadStartTime();

      // <spec label="Parsing" step="1">Block this instance of the XML parser,
      // such that the event loop will not run tasks that invoke it.</spec>
      //
      // This is done in XMLDocumentParser::EndElementNs().

      // <spec label="Parsing" step="2">Spin the event loop until the parser's
      // Document has no style sheet that is blocking scripts and the pending
      // parsing-blocking script's ready to be parser-executed is true.</spec>

      // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
      // is blocking scripts.
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
}

}  // namespace blink
