// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_set_html_unsafe_options.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {
class HTMLSink : public UnderlyingSinkBase {
 public:
  explicit HTMLSink(ContainerNode& new_target,
                    SetHTMLUnsafeOptions* new_options)
      : target(new_target), options(new_options) {
    CHECK(target->IsElementNode() || target->IsShadowRoot());
  }

  void Trace(Visitor* visitor) const override {
    UnderlyingSinkBase::Trace(visitor);
    visitor->Trace(target);
    visitor->Trace(parser);
    visitor->Trace(options);
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

    // TODO(nrosenthal): support safe sanitizer.
    StreamingSanitizer* sanitizer =
        SanitizerAPI::CreateStreamingSanitizerUnsafeInternal(options, target,
                                                             exception_state);
    // FIXME(nrosenthal): support more methods. This currently assumes "append".
    // FIXME(nrosenthal): custom element registry support?
    parser = MakeGarbageCollected<HTMLDocumentParser>(
        target, context_element,
        options->runScripts()
            ? ParserContentPolicy::
                  kAllowScriptingContentAndDoNotMarkAlreadyStarted
            : ParserContentPolicy::kAllowScriptingContent,
        ParserPrefetchPolicy::kDisallowPrefetching, /*registry*/ nullptr,
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
  Member<SetHTMLUnsafeOptions> options;
};
}  // namespace

// static
WritableStream* HTMLStream::Create(ScriptState* script_state,
                                   ContainerNode* target,
                                   SetHTMLUnsafeOptions* options,
                                   ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());
  HTMLSink* sink = MakeGarbageCollected<HTMLSink>(*target, options);
  return WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1);
}
}  // namespace blink
