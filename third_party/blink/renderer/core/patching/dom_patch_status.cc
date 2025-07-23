// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/dom_patch_status.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/patching/patch_event.h"
#include "third_party/blink/renderer/core/patching/patch_supplement.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"

namespace blink {
// static
DOMPatchStatus* DOMPatchStatus::Create(ContainerNode& target,
                                       HTMLTemplateElement* source) {
  return MakeGarbageCollected<DOMPatchStatus>(source, target);
}

DOMPatchStatus::DOMPatchStatus(HTMLTemplateElement* source,
                               ContainerNode& target)
    : source_(source),
      target_(target),
      finished_(
          MakeGarbageCollected<ScriptPromiseProperty<IDLUndefined, IDLAny>>(
              target.GetDocument().GetExecutionContext())) {}

ScriptPromise<IDLUndefined> DOMPatchStatus::finished(
    ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void DOMPatchStatus::Start() {
  if (state_ != State::kPending) {
    return;
  }
  state_ = State::kActive;
  // A patch replaces the existing children of the target.
  target_->RemoveChildren();
  MutationObserver::EnqueuePatch(*this);
  PatchSupplement::From(GetDocument())->DidStart(*target_, this);
  parser_ = MakeGarbageCollected<HTMLDocumentParser>(
      target_,
      target_->IsElementNode() ? &To<Element>(*target_)
                               : target_->parentElement(),
      ParserContentPolicy::kDisallowScriptingAndPluginContent);
  if (parser_->NeedsDecoder()) {
    parser_->SetDecoder(
        std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::ContentType::kHTMLContent,
            GetDocument().Encoding())));
  }
}

void DOMPatchStatus::DispatchPatchEvent() {
  Event* event =
      MakeGarbageCollected<PatchEvent>(event_type_names::kPatch, this);
  event->SetTarget(target_);
  target_->DispatchEvent(*event);
}

void DOMPatchStatus::Finish() {
  if (state_ != State::kActive) {
    CHECK_EQ(state_, State::kTerminated);
    return;
  }
  state_ = State::kFinished;
  parser_->Finish();
  finished_->ResolveWithUndefined();
  PatchSupplement::From(GetDocument())->DidComplete(*target_);
}

Document& DOMPatchStatus::GetDocument() {
  return target_->GetDocument();
}

void DOMPatchStatus::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(target_);
  visitor->Trace(finished_);
  visitor->Trace(parser_);
  ScriptWrappable::Trace(visitor);
}

void DOMPatchStatus::Append(const String& text) {
  if (state_ == State::kActive) {
    parser_->Append(text);
  }
}

void DOMPatchStatus::Terminate(ScriptValue reason) {
  if (state_ == State::kFinished || state_ == State::kTerminated) {
    return;
  }

  state_ = State::kTerminated;

  finished_->Reject(reason);
  parser_->Finish();
  PatchSupplement::From(GetDocument())->DidComplete(*target_);
}

void DOMPatchStatus::AppendBytes(base::span<uint8_t> bytes) {
  parser_->AppendBytes(bytes);
}

}  // namespace blink
