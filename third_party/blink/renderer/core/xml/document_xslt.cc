// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/document_xslt.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/xml/xsl_style_sheet.h"
#include "third_party/blink/renderer/core/xml/xslt_processor.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

class DOMContentLoadedListener final
    : public NativeEventListener,
      public ProcessingInstruction::DetachableEventListener {
  USING_GARBAGE_COLLECTED_MIXIN(DOMContentLoadedListener);

 public:
  explicit DOMContentLoadedListener(ProcessingInstruction* pi)
      : processing_instruction_(pi) {}

  void Invoke(ExecutionContext* execution_context, Event* event) override {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    DCHECK_EQ(event->type(), "DOMContentLoaded");

    Document& document = *To<Document>(execution_context);
    DCHECK(!document.Parsing());

    // Processing instruction (XML documents only).
    // We don't support linking to embedded CSS stylesheets,
    // see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
    // Don't apply XSL transforms to already transformed documents.
    if (DocumentXSLT::HasTransformSourceDocument(document))
      return;

    ProcessingInstruction* pi = DocumentXSLT::FindXSLStyleSheet(document);
    if (!pi || pi != processing_instruction_ || pi->IsLoading())
      return;
    DocumentXSLT::ApplyXSLTransform(document, pi);
  }

  void Detach() override { processing_instruction_ = nullptr; }

  EventListener* ToEventListener() override { return this; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(processing_instruction_);
    NativeEventListener::Trace(visitor);
    ProcessingInstruction::DetachableEventListener::Trace(visitor);
  }

 private:
  // If this event listener is attached to a ProcessingInstruction, keep a
  // weak reference back to it. That ProcessingInstruction is responsible for
  // detaching itself and clear out the reference.
  Member<ProcessingInstruction> processing_instruction_;
};

DocumentXSLT::DocumentXSLT(Document& document)
    : Supplement<Document>(document), transform_source_document_(nullptr) {}

void DocumentXSLT::ApplyXSLTransform(Document& document,
                                     ProcessingInstruction* pi) {
  DCHECK(!pi->IsLoading());
  UseCounter::Count(document, WebFeature::kXSLProcessingInstruction);
  XSLTProcessor* processor = XSLTProcessor::Create(document);
  processor->SetXSLStyleSheet(ToXSLStyleSheet(pi->sheet()));
  String result_mime_type;
  String new_source;
  String result_encoding;
  document.SetParsingState(Document::kParsing);
  if (!processor->TransformToString(&document, result_mime_type, new_source,
                                    result_encoding)) {
    document.SetParsingState(Document::kFinishedParsing);
    return;
  }
  // FIXME: If the transform failed we should probably report an error (like
  // Mozilla does).
  LocalFrame* owner_frame = document.GetFrame();
  processor->CreateDocumentFromSource(new_source, result_encoding,
                                      result_mime_type, &document, owner_frame);
  probe::FrameDocumentUpdated(owner_frame);
  document.SetParsingState(Document::kFinishedParsing);
}

ProcessingInstruction* DocumentXSLT::FindXSLStyleSheet(Document& document) {
  for (Node* node = document.firstChild(); node; node = node->nextSibling()) {
    auto* pi = DynamicTo<ProcessingInstruction>(node);
    if (pi && pi->IsXSL())
      return pi;
  }
  return nullptr;
}

bool DocumentXSLT::ProcessingInstructionInsertedIntoDocument(
    Document& document,
    ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (!RuntimeEnabledFeatures::XSLTEnabled() || !document.GetFrame())
    return true;

  auto* listener = MakeGarbageCollected<DOMContentLoadedListener>(pi);
  document.addEventListener(event_type_names::kDOMContentLoaded, listener,
                            false);
  DCHECK(!pi->EventListenerForXSLT());
  pi->SetEventListenerForXSLT(listener);
  return true;
}

bool DocumentXSLT::ProcessingInstructionRemovedFromDocument(
    Document& document,
    ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (!pi->EventListenerForXSLT())
    return true;

  DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
  document.removeEventListener(event_type_names::kDOMContentLoaded,
                               pi->EventListenerForXSLT(), false);
  pi->ClearEventListenerForXSLT();
  return true;
}

bool DocumentXSLT::SheetLoaded(Document& document, ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (RuntimeEnabledFeatures::XSLTEnabled() && !document.Parsing() &&
      !pi->IsLoading() && !DocumentXSLT::HasTransformSourceDocument(document)) {
    if (FindXSLStyleSheet(document) == pi)
      ApplyXSLTransform(document, pi);
  }
  return true;
}

// static
const char DocumentXSLT::kSupplementName[] = "DocumentXSLT";

bool DocumentXSLT::HasTransformSourceDocument(Document& document) {
  return Supplement<Document>::From<DocumentXSLT>(document);
}

DocumentXSLT& DocumentXSLT::From(Document& document) {
  DocumentXSLT* supplement = Supplement<Document>::From<DocumentXSLT>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<DocumentXSLT>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return *supplement;
}

void DocumentXSLT::Trace(blink::Visitor* visitor) {
  visitor->Trace(transform_source_document_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
