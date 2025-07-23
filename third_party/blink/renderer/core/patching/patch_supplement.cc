// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/patch_supplement.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/patching/dom_patch_status.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "v8-primitive.h"

namespace blink {

// static
const char PatchSupplement::kSupplementName[] = "Patch";

namespace {
class SinglePatchSink : public UnderlyingSinkBase {
 public:
  explicit SinglePatchSink(ContainerNode& target)
      : patch_(DOMPatchStatus::Create(target)) {}
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
    v8::Local<v8::Value> value = chunk.V8ValueFor(script_state);
    if (value->IsArrayBuffer() || value->IsArrayBufferView()) {
      V8BufferSource* source = V8BufferSource::Create(
          script_state->GetIsolate(), chunk.V8Value(), exception_state);
      // TODO(nrosenthal): find cases where this can legitimately fail.
      CHECK(!exception_state.HadException());
      patch_->AppendBytes(DOMArrayPiece(source).ByteSpan());
    } else {
      String chunk_as_string;
      if (value->IsSymbol() || !chunk.ToString(chunk_as_string)) {
        auto* exception = DOMException::Create(
            "Patch stream only accepts byte buffers or values that can be "
            "stringified",
            DOMException::GetErrorName(DOMExceptionCode::kDataError));
        patch_->Terminate(ScriptValue::From(script_state, exception));
        return ScriptPromise<IDLUndefined>::RejectWithDOMException(script_state,
                                                                   exception);
      }
      patch_->Append(chunk_as_string);
    }
    return ToResolvedUndefinedPromise(script_state);
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

  Member<DOMPatchStatus> patch_;
};
}  // namespace

// static
PatchSupplement* PatchSupplement::FromIfExists(const Document& document) {
  return Supplement<Document>::From<PatchSupplement>(document);
}

// static
PatchSupplement* PatchSupplement::From(Document& document) {
  auto* supplement = Supplement<Document>::From<PatchSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<PatchSupplement>(document);
    Supplement<Document>::ProvideTo(document, supplement);
  }
  return supplement;
}

DOMPatchStatus* PatchSupplement::CurrentPatchFor(const Node& target) {
  if (auto index = IndexOfPatch(target)) {
    return patches_.at(*index);
  } else {
    return nullptr;
  }
}

std::optional<size_t> PatchSupplement::IndexOfPatch(const Node& target) {
  for (size_t i = 0; i < patches_.size(); ++i) {
    if (patches_[i]->GetTarget() == target) {
      return i;
    }
  }
  return std::nullopt;
}

void PatchSupplement::DidStart(Node& target, DOMPatchStatus* status) {
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
    ContainerNode& target) {
  DOMPatchStatus* previous = CurrentPatchFor(target);
  if (previous) {
    previous->Terminate(ScriptValue::From(
        script_state,
        DOMException::Create(
            "Patch aborted by another patch call",
            DOMException::GetErrorName(DOMExceptionCode::kAbortError))));
  };
  return WritableStream::CreateWithCountQueueingStrategy(
      script_state, MakeGarbageCollected<SinglePatchSink>(target), 1);
}

void PatchSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(patches_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
