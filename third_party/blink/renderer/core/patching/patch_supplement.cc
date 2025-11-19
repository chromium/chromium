// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/patch_supplement.h"

#include <cstdint>
#include <optional>

#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/patching/patch.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "v8-primitive.h"

namespace blink {

namespace {

std::variant<String, base::span<uint8_t>, std::monostate>
ExtractBytesOrStringFromChunk(ScriptValue chunk,
                              ScriptState* script_state,
                              ExceptionState& exception_state) {
  v8::Local<v8::Value> value = chunk.V8ValueFor(script_state);
  if (value->IsArrayBuffer() || value->IsArrayBufferView()) {
    V8BufferSource* source = V8BufferSource::Create(
        script_state->GetIsolate(), chunk.V8Value(), exception_state);
    // TODO(nrosenthal): find cases where this can legitimately fail.
    CHECK(!exception_state.HadException());
    return DOMArrayPiece(source).ByteSpan();
  }

  String chunk_as_string;
  if (value->IsSymbol() || !chunk.ToString(chunk_as_string)) {
    return std::monostate();
  }
  return chunk_as_string;
}

class SinglePatchSink : public UnderlyingSinkBase {
 public:
  explicit SinglePatchSink(ContainerNode& target, Node* a, Node* b)
      : patch_(Patch::Create(target, nullptr, KURL(), a, b)) {}
  void Trace(Visitor* visitor) const override {
    visitor->Trace(patch_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override {
    patch_->Start();
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    return std::visit(
        absl::Overload{
            [&](const String& str) {
              patch_->Append(str);
              return ToResolvedUndefinedPromise(script_state);
            },
            [&](base::span<uint8_t> bytes) {
              patch_->AppendBytes(bytes);
              return ToResolvedUndefinedPromise(script_state);
            },
            [&](std::monostate) {
              auto* exception = MakeGarbageCollected<DOMException>(
                  DOMExceptionCode::kDataError,
                  "Patch stream only accepts byte buffers or values that can "
                  "be stringified");
              patch_->Terminate(ScriptValue::From(script_state, exception));
              return ScriptPromise<IDLUndefined>::RejectWithDOMException(
                  script_state, exception);
            }},
        ExtractBytesOrStringFromChunk(chunk, script_state, exception_state));
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    patch_->Finish();
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState&) override {
    patch_->Terminate(reason);
    return ToResolvedUndefinedPromise(script_state);
  }

  Member<Patch> patch_;
};
}  // namespace

namespace {
class SubtreePatchSink : public UnderlyingSinkBase {
 public:
  explicit SubtreePatchSink(ContainerNode& root) : root_(root) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(root_);
    visitor->Trace(parser_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override {
    // We're creating a detached template element as it would initiate
    // the parser in fragment parsing mode without any context-specific rules.
    // The template element is discarded and only the patches are applied to the
    // patch target scope (the node which patchAll was called on).
    HTMLTemplateElement* sink_template =
        MakeGarbageCollected<HTMLTemplateElement>(root_->GetDocument());
    HTMLDocumentParser* parser = MakeGarbageCollected<HTMLDocumentParser>(
        sink_template->content(), sink_template,
        ParserContentPolicy::kDisallowScriptingAndPluginContent,
        ParserPrefetchPolicy::kDisallowPrefetching, /*registry*/ nullptr);
    parser_ = parser;
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    if (!parser_) {
      return ScriptPromise<IDLUndefined>::RejectWithDOMException(
          script_state,
          // TODO(nrodsenthal): add test
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kInvalidStateError, "Patch is closed"));
    }
    return std::visit(
        absl::Overload{
            [&](const String& str) {
              parser_->Append(str);
              return ToResolvedUndefinedPromise(script_state);
            },
            [&](base::span<uint8_t> bytes) {
              if (parser_->NeedsDecoder()) {
                parser_->SetDecoder(std::make_unique<TextResourceDecoder>(
                    TextResourceDecoderOptions(
                        TextResourceDecoderOptions::ContentType::kHTMLContent,
                        root_->GetDocument().Encoding())));
              }
              parser_->AppendBytes(bytes);
              return ToResolvedUndefinedPromise(script_state);
            },
            [&](std::monostate) {
              auto* exception = MakeGarbageCollected<DOMException>(
                  DOMExceptionCode::kDataError,
                  "Patch stream only accepts byte buffers or values that can "
                  "be stringified");
              parser_.Clear();
              return ScriptPromise<IDLUndefined>::RejectWithDOMException(
                  script_state, exception);
            }},
        ExtractBytesOrStringFromChunk(chunk, script_state, exception_state));
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    parser_.Clear();
    return ToResolvedUndefinedPromise(script_state);
  }
  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState&) override {
    parser_.Clear();
    return ToResolvedUndefinedPromise(script_state);
  }

  Member<ContainerNode> root_;
  Member<DocumentParser> parser_;
};

}  // namespace

// static
PatchSupplement* PatchSupplement::FromIfExists(const Document& document) {
  return document.GetPatchSupplement();
}

// static
PatchSupplement* PatchSupplement::From(Document& document) {
  auto* supplement = document.GetPatchSupplement();
  if (!supplement) {
    supplement = MakeGarbageCollected<PatchSupplement>();
    document.SetPatchSupplement(supplement);
  }
  return supplement;
}

Patch* PatchSupplement::CurrentPatchFor(const Node& target) {
  if (auto index = IndexOfPatch(target)) {
    return patches_.at(*index);
  } else {
    return nullptr;
  }
}

std::optional<size_t> PatchSupplement::IndexOfPatch(const Node& target) {
  for (size_t i = 0; i < patches_.size(); ++i) {
    if (patches_[i]->Target() == target) {
      return i;
    }
  }
  return std::nullopt;
}

void PatchSupplement::DidStart(Node& target, Patch* status) {
  patches_.push_back(status);
  if (Element* element = DynamicTo<Element>(target)) {
    element->PatchStateChanged();
  }
}

void PatchSupplement::DidComplete(Node& target) {
  if (auto index = IndexOfPatch(target)) {
    patches_.EraseAt(*index);
  }
  if (Element* element = DynamicTo<Element>(target)) {
    element->PatchStateChanged();
  }
}

WritableStream* PatchSupplement::CreateSinglePatchStream(
    ScriptState* script_state,
    ContainerNode& target,
    Node* previous_child,
    Node* next_child) {
  Patch* previous = CurrentPatchFor(target);
  if (previous) {
    previous->Terminate(ScriptValue::From(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kAbortError,
                          "Patch aborted by another patch call")));
  };
  return WritableStream::CreateWithCountQueueingStrategy(
      script_state,
      MakeGarbageCollected<SinglePatchSink>(target, previous_child, next_child),
      1);
}

WritableStream* PatchSupplement::CreateSubtreePatchStream(
    ScriptState* script_state,
    ContainerNode& target) {
  return WritableStream::CreateWithCountQueueingStrategy(
      script_state, MakeGarbageCollected<SubtreePatchSink>(target), 1);
}

void PatchSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(patches_);
}

}  // namespace blink
