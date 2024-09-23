/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/document.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedhtml.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

EditorCommand GetCommand(Document* document, const String& command_name) {
  LocalFrame* frame = document->GetFrame();
  if (!frame || frame->GetDocument() != document)
    return EditorCommand();

  document->UpdateStyleAndLayoutTree();
  return frame->GetEditor().CreateCommand(command_name,
                                          EditorCommandSource::kDOM);
}

// Trusted Types requires that HTML (or Script, or script URLs) to be
// inserted into a Document go through a Trusted Types check first. This
// is a slightly awkward fit for execCommand API structure, which effectively
// dispatches to very different code based on its command name. Here, we'll
// check whether we need to run a Trusted Types check in the first place, and
// will also run the check if necessary.
String TrustedTypesCheck(Document* document,
                         const EditorCommand& editor_command,
                         const V8UnionStringOrTrustedHTML* value,
                         ExceptionState& exception_state) {
  // If we receive null or the value parameter is missing, then there's nothing
  // to check.
  if (!value)
    return g_empty_string;

  // TrustedHTML values always pass.
  if (value->IsTrustedHTML())
    return value->GetAsTrustedHTML()->toString();

  // We received a plain string. Most editor commands won't read the value as
  // HTML. Those commands can pass.
  DCHECK(value->IsString());
  if (!editor_command.IsValueInterpretedAsHTML())
    return value->GetAsString();

  // We received plain string, and it's one of the commands of interest.
  // Run the TT check.
  return TrustedTypesCheckForExecCommand(
      value->GetAsString(), document->GetExecutionContext(), exception_state);
}

}  // namespace

bool Document::execCommand(const String& command_name,
                           bool unused_bool,
                           const String& value,
                           ExceptionState& exception_state) {
  V8UnionStringOrTrustedHTML* tmp =
      MakeGarbageCollected<V8UnionStringOrTrustedHTML>(value);
  return execCommand(command_name, unused_bool, tmp, exception_state);
}

bool Document::execCommand(const String& command_name,
                           bool,
                           const V8UnionStringOrTrustedHTML* value,
                           ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "execCommand is only supported on HTML documents.");
    return false;
  }

  UseCounter::Count(*this, WebFeature::kExecCommand);
  if (FocusedElement() && IsTextControl(*FocusedElement()))
    UseCounter::Count(*this, WebFeature::kExecCommandOnInputOrTextarea);

  // We don't allow recursive |execCommand()| to protect against attack code.
  // Recursive call of |execCommand()| could be happened by moving iframe
  // with script triggered by insertion, e.g. <iframe src="javascript:...">
  // <iframe onload="...">. This usage is valid as of the specification
  // although, it isn't common use case, rather it is used as attack code.
  if (is_running_exec_command_) {
    String message =
        "We don't execute document.execCommand() this time, because it is "
        "called recursively.";
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return false;
  }
  base::AutoReset<bool> execute_scope(&is_running_exec_command_, true);

  // Postpone DOM mutation events, which can execute scripts and change
  // DOM tree against implementation assumption.
  EventQueueScope event_queue_scope;
  TidyUpHTMLStructure(*this);
  const EditorCommand editor_command = GetCommand(this, command_name);

  String checked_value =
      TrustedTypesCheck(this, editor_command, value, exception_state);
  if (exception_state.HadException())
    return false;

  base::UmaHistogramSparse("WebCore.Document.execCommand",
                           editor_command.IdForHistogram());
  return editor_command.Execute(checked_value);
}

bool Document::queryCommandEnabled(const String& command_name,
                                   ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "queryCommandEnabled is only supported on HTML documents.");
    return false;
  }

  return GetCommand(this, command_name).IsEnabled();
}

bool Document::queryCommandIndeterm(const String& command_name,
                                    ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "queryCommandIndeterm is only supported on HTML documents.");
    return false;
  }

  return GetCommand(this, command_name).GetState() == EditingTriState::kMixed;
}

bool Document::queryCommandState(const String& command_name,
                                 ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "queryCommandState is only supported on HTML documents.");
    return false;
  }

  return GetCommand(this, command_name).GetState() == EditingTriState::kTrue;
}

bool Document::queryCommandSupported(const String& command_name,
                                     ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "queryCommandSupported is only supported on HTML documents.");
    return false;
  }

  return GetCommand(this, command_name).IsSupported();
}

String Document::queryCommandValue(const String& command_name,
                                   ExceptionState& exception_state) {
  if (!IsHTMLDocument() && !IsXHTMLDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "queryCommandValue is only supported on HTML documents.");
    return "";
  }

  return GetCommand(this, command_name).Value();
}

}  // namespace blink
