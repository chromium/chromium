// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_stream.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
  explicit HTMLSink(ContainerNode& target,
                    Node* ref_node,
                    Sanitizer::Mode sanitizer_mode,
                    FragmentParserOptions new_options,
                    ExceptionState& exception_state)
      : root_insertion_point(
            MakeGarbageCollected<ParserRootInsertionPoint>(target, ref_node)),
        sanitizer(SanitizerAPI::CreateStreamingSanitizer(
            sanitizer_mode,
            StreamingSanitizer::TextNodeMergeMode::kMerge,
            new_options,
            exception_state)),
        parser_content_policy(
            new_options.run_scripts() ==
                    FragmentParserOptions::RunScripts::kRunScripts
                ? ParserContentPolicy::
                      kAllowScriptingContentAndMarkAsParserInserted
                : ParserContentPolicy::kAllowScriptingContent) {
    CHECK(root_insertion_point->target->IsElementNode() ||
          root_insertion_point->target->IsShadowRoot());
  }

  void Trace(Visitor* visitor) const override {
    UnderlyingSinkBase::Trace(visitor);
    visitor->Trace(root_insertion_point);
    visitor->Trace(parser);
    visitor->Trace(sanitizer);
  }

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    ContainerNode* target = root_insertion_point->target;
    Element* context_element = DynamicTo<Element>(target);
    if (!context_element) {
      if (ShadowRoot* shadow = DynamicTo<ShadowRoot>(target)) {
        context_element = &shadow->host();
      }
    }

    // TODO(nrosenthal): use an inert document to avoid pre-sanitization side
    // effects.
    CustomElementRegistry* registry = context_element->customElementRegistry();

    target->GetDocument().setAllowDeclarativeShadowRoots(true);

    parser = MakeGarbageCollected<HTMLDocumentParser>(
        target->GetDocument().createDocumentFragment(), context_element,
        parser_content_policy, ParserPrefetchPolicy::kDisallowPrefetching,
        registry, sanitizer, root_insertion_point);

    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    CHECK(root_insertion_point);
    CHECK(root_insertion_point->target);
    CHECK(parser);
    if (chunk.V8ValueFor(script_state)->IsSymbol()) {
      exception_state.ThrowTypeError("Cannot stream symbols into HTML");
      return ToResolvedUndefinedPromise(script_state);
    }

    String text;
    const bool chunk_stringified = chunk.ToString(text);
    CHECK(chunk_stringified);
    if (root_insertion_point->ref_node &&
        root_insertion_point->ref_node->parentNode() !=
            root_insertion_point->target) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kHierarchyRequestError,
          "The ref_node is no longer a child of the target.");
      return ToResolvedUndefinedPromise(script_state);
    }
    parser->Append(text);
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    if (parser) {
      parser->Finish();
      parser = nullptr;
    }

    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState& exception_state) override {
    return close(script_state, exception_state);
  }

  Member<ParserRootInsertionPoint> root_insertion_point;
  Member<DocumentParser> parser;
  Member<StreamingSanitizer> sanitizer;
  ParserContentPolicy parser_content_policy;
};
}  // namespace

// static
WritableStream* HTMLStream::Create(ScriptState* script_state,
                                   ContainerNode* target,
                                   Node* ref_node,
                                   Sanitizer::Mode sanitizer_mode,
                                   const FragmentParserOptions& options,
                                   const AtomicString& interface_name,
                                   const AtomicString& property_name,
                                   ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::NewHTMLSettingMethodsEnabled());

  CHECK(!ref_node || ref_node->parentNode() == target);

  if (!target) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Cannot stream before/after a node with a null parent");
    return nullptr;
  }

  if (!target->IsElementNode() && !target->IsShadowRoot()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Cannot stream before/after a node that "
                                      "is not an element or shadow root");
    return nullptr;
  }

  std::optional<FragmentParserOptions> trusted_options =
      sanitizer_mode == Sanitizer::Mode::kSafe
          ? std::make_optional(options)
          : TrustedTypesCheckForParserOptions(
                options, MarkupInsertionMode::kStream,
                target->GetExecutionContext(), interface_name, property_name,
                exception_state);

  if (!trusted_options) {
    return nullptr;
  }

  HTMLSink* sink = MakeGarbageCollected<HTMLSink>(
      *target, ref_node, sanitizer_mode, *trusted_options, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1);
}

}  // namespace blink
