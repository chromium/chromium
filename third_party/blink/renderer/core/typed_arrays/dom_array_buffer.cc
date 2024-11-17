// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include <algorithm>

#include "base/containers/buffer_iterator.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// Construction of WrapperTypeInfo may require non-trivial initialization due
// to cross-component address resolution in order to load the pointer to the
// parent interface's WrapperTypeInfo.  We ignore this issue because the issue
// happens only on component builds and the official release builds
// (statically-linked builds) are never affected by this issue.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

const WrapperTypeInfo DOMArrayBuffer::wrapper_type_info_body_{
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "ArrayBuffer",
    nullptr,
    kDOMWrappersTag,
    kDOMWrappersTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlBufferSourceType,
};

const WrapperTypeInfo& DOMArrayBuffer::wrapper_type_info_ =
    DOMArrayBuffer::wrapper_type_info_body_;

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {

template <typename Function>
void ForArrayBuffersInAllWorlds(v8::Isolate* isolate,
                                const DOMArrayBuffer* object,
                                Function f) {
  if (!object->has_non_main_world_wrappers() && IsMainThread()) {
    const DOMWrapperWorld& world = DOMWrapperWorld::MainWorld(isolate);
    v8::Local<v8::Object> wrapper;
    if (world.DomDataStore()
            .Get</*entered_context=*/false>(isolate, object)
            .ToLocal(&wrapper)) {
      f(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
    return;
  }

  HeapVector<Member<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInIsolate(isolate, worlds);
  for (const auto& world : worlds) {
    v8::Local<v8::Object> wrapper;
    if (world->DomDataStore()
            .Get</*entered_context=*/false>(isolate, object)
            .ToLocal(&wrapper)) {
      f(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
    }
  }
}

}  // namespace

bool DOMArrayBuffer::IsDetachable(v8::Isolate* isolate) {
  v8::HandleScope handle_scope(isolate);
  v8::LocalVector<v8::ArrayBuffer> buffer_handles(isolate);
  bool is_detachable = true;
  ForArrayBuffersInAllWorlds(
      isolate, this,
      [&is_detachable](v8::Local<v8::ArrayBuffer> buffer_handle) {
        is_detachable &= buffer_handle->IsDetachable();
      });
  return is_detachable;
}

void DOMArrayBuffer::SetDetachKey(v8::Isolate* isolate,
                                  const StringView& detach_key) {
  // It's easy to support setting a detach key multiple times, but it's very
  // likely to be a program error to set a detach key multiple times.
  DCHECK(detach_key_.IsEmpty());

  v8::HandleScope handle_scope(isolate);
  v8::LocalVector<v8::ArrayBuffer> buffer_handles(isolate);

  v8::Local<v8::String> v8_detach_key = V8AtomicString(isolate, detach_key);
  detach_key_.Reset(isolate, v8_detach_key);

  ForArrayBuffersInAllWorlds(
      isolate, this,
      [&v8_detach_key](v8::Local<v8::ArrayBuffer> buffer_handle) {
        buffer_handle->SetDetachKey(v8_detach_key);
      });
}

bool DOMArrayBuffer::Transfer(v8::Isolate* isolate,
                              ArrayBufferContents& result,
                              ExceptionState& exception_state) {
  return Transfer(isolate, v8::Local<v8::Value>(), result, exception_state);
}

bool DOMArrayBuffer::Transfer(v8::Isolate* isolate,
                              v8::Local<v8::Value> detach_key,
                              ArrayBufferContents& result,
                              ExceptionState& exception_state) {
  DOMArrayBuffer* to_transfer = this;
  if (!IsDetachable(isolate)) {
    to_transfer = DOMArrayBuffer::Create(Content()->ByteSpan());
  }

  TryRethrowScope rethrow_scope(isolate, exception_state);
  bool detach_result = false;
  if (!to_transfer->TransferDetachable(isolate, detach_key, result)
           .To(&detach_result)) {
    return false;
  }
  if (!detach_result) {
    exception_state.ThrowTypeError("Could not transfer ArrayBuffer.");
    return false;
  }
  return true;
}

bool DOMArrayBuffer::ShareNonSharedForInternalUse(ArrayBufferContents& result) {
  if (!Content()->BackingStore()) {
    result.Detach();
    return false;
  }
  Content()->ShareNonSharedForInternalUse(result);
  return true;
}

v8::Maybe<bool> DOMArrayBuffer::TransferDetachable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> detach_key,
    ArrayBufferContents& result) {
  DCHECK(IsDetachable(isolate));

  if (IsDetached()) {
    result.Detach();
    return v8::Just(false);
  }

  if (!Content()->Data()) {
    // We transfer an empty ArrayBuffer, we can just allocate an empty content.
    result = ArrayBufferContents(Content()->BackingStore());
  } else {
    Content()->Transfer(result);
  }

  v8::HandleScope handle_scope(isolate);
  v8::LocalVector<v8::ArrayBuffer> buffer_handles(isolate);

  bool first = true;
  bool failed = false;
  ForArrayBuffersInAllWorlds(
      isolate, this,
      [&first, &failed, &detach_key](v8::Local<v8::ArrayBuffer> buffer_handle) {
        // Loop to detach all buffer handles. This may throw an exception
        // if the |detach_key| is incorrect. It should either fail for all
        // handles or succeed for all handles. It should never be the case that
        // the handles have different detach keys. CHECK to catch when this
        // invariant is broken.
        if (!failed) {
          bool detach_result = false;
          if (!buffer_handle->Detach(detach_key).To(&detach_result)) {
            CHECK(first);
            failed = true;
          } else {
            // On success, Detach must always return true.
            DCHECK(detach_result);
          }
          first = false;
        }
      });

  if (failed) {
    // Propagate an exception to the caller.
    return v8::Nothing<bool>();
  }

  Detach();
  return v8::Just(true);
}

DOMArrayBuffer* DOMArrayBuffer::Create(
    scoped_refptr<SharedBuffer> shared_buffer) {
  ArrayBufferContents contents(
      shared_buffer->size(), 1, ArrayBufferContents::kNotShared,
      ArrayBufferContents::kDontInitialize,
      ArrayBufferContents::AllocationFailureBehavior::kCrash);
  CHECK(contents.IsValid());

  base::BufferIterator iterator(contents.ByteSpan());
  for (const auto& span : *shared_buffer) {
    iterator.MutableSpan<char>(span.size()).copy_from(span);
  }

  return Create(std::move(contents));
}

DOMArrayBuffer* DOMArrayBuffer::Create(
    const Vector<base::span<const char>>& data) {
  size_t size = 0;
  for (const auto& span : data) {
    size += span.size();
  }
  ArrayBufferContents contents(
      size, 1, ArrayBufferContents::kNotShared,
      ArrayBufferContents::kDontInitialize,
      ArrayBufferContents::AllocationFailureBehavior::kCrash);
  CHECK(contents.IsValid());

  base::BufferIterator iterator(contents.ByteSpan());
  for (const auto& span : data) {
    iterator.MutableSpan<char>(span.size()).copy_from(span);
  }

  return Create(std::move(contents));
}

DOMArrayBuffer* DOMArrayBuffer::CreateOrNull(size_t num_elements,
                                             size_t element_byte_size) {
  ArrayBufferContents contents(num_elements, element_byte_size,
                               ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kZeroInitialize);
  if (!contents.Data()) {
    return nullptr;
  }
  return Create(std::move(contents));
}

DOMArrayBuffer* DOMArrayBuffer::CreateOrNull(base::span<const uint8_t> source) {
  DOMArrayBuffer* buffer = CreateUninitializedOrNull(source.size(), 1);
  if (!buffer) {
    return nullptr;
  }

  buffer->ByteSpan().copy_from(source);
  return buffer;
}

DOMArrayBuffer* DOMArrayBuffer::CreateUninitializedOrNull(
    size_t num_elements,
    size_t element_byte_size) {
  ArrayBufferContents contents(num_elements, element_byte_size,
                               ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kDontInitialize);
  if (!contents.Data()) {
    return nullptr;
  }
  return Create(std::move(contents));
}

v8::Local<v8::Value> DOMArrayBuffer::Wrap(ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(script_state->GetIsolate(), this));

  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();

  v8::Local<v8::ArrayBuffer> wrapper;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    std::shared_ptr<v8::BackingStore> backing_store = Content()->BackingStore();
    wrapper = backing_store
                  ? v8::ArrayBuffer::New(script_state->GetIsolate(),
                                         std::move(backing_store))
                  : v8::ArrayBuffer::New(script_state->GetIsolate(), 0);

    if (!detach_key_.IsEmpty()) {
      wrapper->SetDetachKey(detach_key_.Get(script_state->GetIsolate()));
    }
  }

  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

bool DOMArrayBuffer::IsDetached() const {
  if (contents_.BackingStore() == nullptr) {
    return is_detached_;
  }
  if (is_detached_) {
    return true;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  // There may be several v8::ArrayBuffers corresponding to the DOMArrayBuffer,
  // but at most one of them may be non-detached.
  int nondetached_count = 0;
  int detached_count = 0;

  ForArrayBuffersInAllWorlds(isolate, this,
                             [&detached_count, &nondetached_count](
                                 v8::Local<v8::ArrayBuffer> buffer_handle) {
                               if (buffer_handle->WasDetached()) {
                                 ++detached_count;
                               } else {
                                 ++nondetached_count;
                               }
                             });

  // This CHECK fires even though it should not. TODO(330759272): Investigate
  // under which conditions we end up with multiple non-detached JSABs for the
  // same DOMAB and potentially restore this check.

  // CHECK_LE(nondetached_count, 1);

  return nondetached_count == 0 && detached_count > 0;
}

v8::Local<v8::Object> DOMArrayBuffer::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  if (!DOMWrapperWorld::Current(isolate).IsMainWorld()) {
    has_non_main_world_wrappers_ = true;
  }
  return ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type_info,
                                               wrapper);
}

DOMArrayBuffer* DOMArrayBuffer::Slice(size_t begin, size_t end) const {
  begin = std::min(begin, ByteLength());
  end = std::min(end, ByteLength());
  size_t size = begin <= end ? end - begin : 0;
  return Create(ByteSpan().subspan(begin, size));
}

void DOMArrayBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(detach_key_);
  DOMArrayBufferBase::Trace(visitor);
}

}  // namespace blink
