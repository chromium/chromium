// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/template_content_document_fragment.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/fragment_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {
class HTMLSink : public UnderlyingSinkBase {
 public:
  HTMLSink(DocumentParser* parser,
           ParserRootInsertionPoint* root_insertion_point)
      : parser_(parser), root_insertion_point_(root_insertion_point) {}

  void Trace(Visitor* visitor) const override {
    UnderlyingSinkBase::Trace(visitor);
    visitor->Trace(parser_);
    visitor->Trace(root_insertion_point_);
  }

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    CHECK(parser_);
    CHECK(root_insertion_point_);
    if (chunk.V8ValueFor(script_state)->IsSymbol()) {
      exception_state.ThrowTypeError("Cannot stream symbols into HTML");
      return ToResolvedUndefinedPromise(script_state);
    }

    const String text = NativeValueTraits<IDLString>::NativeValue(
        script_state->GetIsolate(), chunk.V8ValueFor(script_state),
        exception_state);

    if (exception_state.HadException()) {
      return ToResolvedUndefinedPromise(script_state);
    }

    if (root_insertion_point_->ref_node &&
        root_insertion_point_->ref_node->parentNode() !=
            root_insertion_point_->target) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kHierarchyRequestError,
          "The ref_node is no longer a child of the target.");
      return ToResolvedUndefinedPromise(script_state);
    }

    parser_->Append(text);
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    parser_->Finish();
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState& exception_state) override {
    parser_->StopParsing();
    return ToResolvedUndefinedPromise(script_state);
  }

  Member<DocumentParser> parser_;
  Member<ParserRootInsertionPoint> root_insertion_point_;
};
}  // namespace

// static
WritableStream* HTMLStream::Create(ScriptState* script_state,
                                   ContainerNode* target,
                                   Node* ref_node,
                                   Sanitizer::Mode sanitizer_mode,
                                   const FragmentParserOptions& options,
                                   ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::NewHTMLSettingMethodsEnabled());

  CHECK(!ref_node || ref_node->parentNode() == target);

  if (!target) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Cannot stream before/after a node with a null parent");
    return nullptr;
  }

  if (IsA<HTMLTemplateElement>(target)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Cannot stream around a direct child of a template element.");
    return nullptr;
  }

  const bool is_template_content =
      target->IsDocumentFragment() &&
      To<DocumentFragment>(target)->IsTemplateContent();

  if (!target->IsElementNode() && !target->IsShadowRoot() &&
      !is_template_content) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Cannot stream before/after a node that "
                                      "is not an element or shadow root");
    return nullptr;
  }

  if (!SanitizerAPI::AllowMutatingRootElement(sanitizer_mode, target)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "Cannot stream safely into a script element");
    return nullptr;
  }

  const ParserContentPolicy parser_content_policy =
      options.run_scripts() == FragmentParserOptions::RunScripts::kRunScripts
          ? ParserContentPolicy::kAllowScriptingContentAndMarkAsParserInserted
          : ParserContentPolicy::kAllowScriptingContent;

  auto* root_insertion_point =
      MakeGarbageCollected<ParserRootInsertionPoint>(*target, ref_node);

  Element* context_element = DynamicTo<Element>(target);
  if (ShadowRoot* shadow = DynamicTo<ShadowRoot>(target)) {
    context_element = &shadow->host();
  } else if (is_template_content) {
    context_element =
        static_cast<TemplateContentDocumentFragment*>(target)->Host();
  }

  CHECK(context_element);

  auto* sanitizer = options.sanitizer_init()
                        ? SanitizerAPI::CreateStreamingSanitizer(
                              sanitizer_mode, options, exception_state)
                        : nullptr;
  if (exception_state.HadException()) {
    return nullptr;
  }

  // TODO(crbug.com/544919880): this call doesn't look correct. It should be a
  // parser flag. See https://github.com/whatwg/html/issues/12652
  target->GetDocument().setAllowDeclarativeShadowRoots(true);

  DocumentParser* parser = MakeGarbageCollected<HTMLDocumentParser>(
      target->GetDocument().createDocumentFragment(), context_element,
      parser_content_policy, ParserPrefetchPolicy::kDisallowPrefetching,
      context_element->customElementRegistry(), sanitizer,
      root_insertion_point);

  return WritableStream::CreateWithCountQueueingStrategy(
      script_state,
      MakeGarbageCollected<HTMLSink>(parser, root_insertion_point), 1);
}

}  // namespace blink
