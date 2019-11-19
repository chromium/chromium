// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/xml_parser_script_runner.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script/xml_parser_script_runner_host.h"

namespace blink {

// Spec links:
// <specdef label="Parsing"
// href="https://html.spec.whatwg.org/C/#parsing-xhtml-documents">
// <specdef label="Prepare"
// href="https://html.spec.whatwg.org/C/#prepare-a-script">

XMLParserScriptRunner::XMLParserScriptRunner(XMLParserScriptRunnerHost* host)
    : host_(host) {}

XMLParserScriptRunner::~XMLParserScriptRunner() {
  DCHECK(!parser_blocking_script_);
}

void XMLParserScriptRunner::Trace(Visitor* visitor) {
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

  CHECK_EQ(pending_script->GetScriptType(), mojom::ScriptType::kClassic);

  // <spec label="Parsing" step="4">Execute the pending parsing-blocking
  // script.</spec>
  pending_script->ExecuteScriptBlock(NullURL());

  // <spec label="Parsing" step="5">There is no longer a pending
  // parsing-blocking script.</spec>
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

  ScriptLoader* script_loader = ScriptLoaderFromElement(element);

  // [Parsing] When the element's end tag is subsequently parsed, the user agent
  // must perform a microtask checkpoint, and then prepare the script element.
  // [spec text]
  bool success = script_loader->PrepareScript(
      script_start_position, ScriptLoader::kAllowLegacyTypeInTypeAttribute);

  if (script_loader->GetScriptType() != mojom::ScriptType::kClassic) {
    // XMLDocumentParser does not support a module script, and thus ignores it.
    success = false;
    document.AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kError,
                               "Module scripts in XML documents are currently "
                               "not supported. See crbug.com/717643"));
  }

  if (!success)
    return;

  // [Parsing] If this causes there to be a pending parsing-blocking script,
  // then the user agent must run the following steps: [spec text]
  if (script_loader->ReadyToBeParserExecuted()) {
    // <spec label="Prepare" step="26.E">... The parser will handle executing
    // the script.</spec>
    //
    // <spec label="Parsing" step="4">Execute the pending parsing-blocking
    // script.</spec>
    //
    // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
    // is blocking scripts and thus the script is executed immediately here,
    // and thus Steps 1-3 are skipped.
    script_loader
        ->TakePendingScript(ScriptSchedulingType::kParserBlockingInline)
        ->ExecuteScriptBlock(document.Url());
  } else if (script_loader->WillBeParserExecuted()) {
    // <spec label="Prepare" step="26.B">... The element is the pending
    // parsing-blocking script of the Document of the parser that created the
    // element. (There can only be one such script per Document at a time.)
    // ...</spec>
    parser_blocking_script_ =
        script_loader->TakePendingScript(ScriptSchedulingType::kParserBlocking);
    parser_blocking_script_->MarkParserBlockingLoadStartTime();

    // <spec label="Parsing" step="1">Block this instance of the XML parser,
    // such that the event loop will not run tasks that invoke it.</spec>
    //
    // This is done in XMLDocumentParser::EndElementNs().

    // <spec label="Parsing" step="2">Spin the event loop until the parser's
    // Document has no style sheet that is blocking scripts and the pending
    // parsing-blocking script's "ready to be parser-executed" flag is
    // set.</spec>

    // TODO(hiroshige): XMLParserScriptRunner doesn't check style sheet that
    // is blocking scripts.
    parser_blocking_script_->WatchForLoad(this);
  }
}

}  // namespace blink
