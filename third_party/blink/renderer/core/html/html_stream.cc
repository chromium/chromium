// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/parser/fragment_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_parser_options.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
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
  explicit HTMLSink(ContainerNode& new_target,
                    FragmentParserOptions new_options)
      : target(new_target),

        sanitizer(SanitizerAPI::CreateStreamingSanitizerInternal(
            new_options,
            &new_target,
            ASSERT_NO_EXCEPTION)),
        parser_content_policy(
            new_options.run_scripts() ==
                    FragmentParserOptions::RunScripts::kRunScripts
                ? ParserContentPolicy::
                      kAllowScriptingContentAndDoNotMarkAlreadyStarted
                : ParserContentPolicy::kAllowScriptingContent) {
    CHECK(target->IsElementNode() || target->IsShadowRoot());
  }

  void Trace(Visitor* visitor) const override {
    UnderlyingSinkBase::Trace(visitor);
    visitor->Trace(target);
    visitor->Trace(parser);
    visitor->Trace(sanitizer);
  }

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    Element* context_element = DynamicTo<Element>(target.Get());
    if (!context_element) {
      if (ShadowRoot* shadow = DynamicTo<ShadowRoot>(target.Get())) {
        context_element = &shadow->host();
      }
    }

    CustomElementRegistry* registry = context_element->customElementRegistry();
    if (!registry) {
      registry = context_element->GetDocument().customElementRegistry();
    }

    // TODO(nrosenthal): support safe sanitizer.
    // FIXME(nrosenthal): support more methods. This currently assumes "append".
    parser = MakeGarbageCollected<HTMLDocumentParser>(
        target, context_element, parser_content_policy,
        ParserPrefetchPolicy::kDisallowPrefetching, registry,
        /*sanitizer*/ sanitizer);

    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    CHECK(target);
    CHECK(parser);
    if (chunk.V8ValueFor(script_state)->IsSymbol()) {
      exception_state.ThrowTypeError("Cannot stream symbols into HTML");
      return ToResolvedUndefinedPromise(script_state);
    }

    String text;
    const bool chunk_stringified = chunk.ToString(text);
    CHECK(chunk_stringified);
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

  Member<ContainerNode> target;
  Member<DocumentParser> parser;
  Member<StreamingSanitizer> sanitizer;
  ParserContentPolicy parser_content_policy;
};
}  // namespace

// static
WritableStream* HTMLStream::Create(ScriptState* script_state,
                                   ContainerNode* target,
                                   FragmentParserOptions options,
                                   const AtomicString& property_name,
                                   ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());

  std::optional<FragmentParserOptions> trusted_options =
      TrustedTypesCheckForParserOptions(
          options, MarkupInsertionMode::kStream, target->GetExecutionContext(),
          target->InterfaceName(), property_name, exception_state);

  if (!trusted_options) {
    return nullptr;
  }

  HTMLSink* sink = MakeGarbageCollected<HTMLSink>(*target, *trusted_options);
  return WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1);
}

}  // namespace blink
