// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
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

static void AccumulateArrayBuffersForAllWorlds(
    v8::Isolate* isolate,
    DOMArrayBuffer* object,
    Vector<v8::Local<v8::ArrayBuffer>, 4>& buffers) {
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  for (const auto& world : worlds) {
    v8::Local<v8::Object> wrapper = world->DomDataStore().Get(object, isolate);
    if (!wrapper.IsEmpty())
      buffers.push_back(v8::Local<v8::ArrayBuffer>::Cast(wrapper));
  }
}

bool DOMArrayBuffer::IsDetachable(v8::Isolate* isolate) {
  Vector<v8::Local<v8::ArrayBuffer>, 4> buffer_handles;
  v8::HandleScope handle_scope(isolate);
  AccumulateArrayBuffersForAllWorlds(isolate, this, buffer_handles);

  bool is_detachable = true;
  for (const auto& buffer_handle : buffer_handles)
    is_detachable &= buffer_handle->IsDetachable();

  return is_detachable;
}

bool DOMArrayBuffer::Transfer(v8::Isolate* isolate,
                              ArrayBufferContents& result) {
  DOMArrayBuffer* to_transfer = this;
  if (!IsDetachable(isolate)) {
    to_transfer = DOMArrayBuffer::Create(Content()->Data(), ByteLength());
  }

  return to_transfer->TransferDetachable(isolate, result);
}

bool DOMArrayBuffer::TransferDetachable(v8::Isolate* isolate,
                                        ArrayBufferContents& result) {
  DCHECK(IsDetachable(isolate));

  if (IsDetached()) {
    result.Detach();
    return false;
  }

  if (!Content()->Data()) {
    // We transfer an empty ArrayBuffer, we can just allocate an empty content.
    result = ArrayBufferContents(Content()->BackingStore());
  } else {
    Content()->Transfer(result);
  }
  // For consistency, the source is detached even if it was empty.
  Detach();

  Vector<v8::Local<v8::ArrayBuffer>, 4> buffer_handles;
  v8::HandleScope handle_scope(isolate);
  AccumulateArrayBuffersForAllWorlds(isolate, this, buffer_handles);

  for (const auto& buffer_handle : buffer_handles)
    buffer_handle->Detach();

  return true;
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

DOMArrayBuffer* DOMArrayBuffer::CreateOrNull(const void* source,
                                             size_t byte_length) {
  DOMArrayBuffer* buffer = CreateUninitializedOrNull(byte_length, 1);
  if (!buffer) {
    return nullptr;
  }

  memcpy(buffer->Data(), source, byte_length);
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

v8::MaybeLocal<v8::Value> DOMArrayBuffer::Wrap(ScriptState* script_state) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, script_state->GetIsolate()));

  const WrapperTypeInfo* wrapper_type_info = GetWrapperTypeInfo();

  v8::Local<v8::ArrayBuffer> wrapper;
  {
    v8::Context::Scope context_scope(script_state->GetContext());
    wrapper = v8::ArrayBuffer::New(script_state->GetIsolate(),
                                   Content()->BackingStore());
  }

  return AssociateWithWrapper(script_state->GetIsolate(), wrapper_type_info,
                              wrapper);
}

DOMArrayBuffer* DOMArrayBuffer::Create(
    scoped_refptr<SharedBuffer> shared_buffer) {
  ArrayBufferContents contents(shared_buffer->size(), 1,
                               ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kDontInitialize);
  uint8_t* data = static_cast<uint8_t*>(contents.Data());
  if (UNLIKELY(!data))
    OOM_CRASH(shared_buffer->size());

  for (const auto& span : *shared_buffer) {
    memcpy(data, span.data(), span.size());
    data += span.size();
  }

  return Create(std::move(contents));
}

DOMArrayBuffer* DOMArrayBuffer::Create(
    const Vector<base::span<const char>>& data) {
  size_t size = 0;
  for (const auto& span : data) {
    size += span.size();
  }
  ArrayBufferContents contents(size, 1, ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kDontInitialize);
  uint8_t* ptr = static_cast<uint8_t*>(contents.Data());
  if (UNLIKELY(!ptr))
    OOM_CRASH(size);

  for (const auto& span : data) {
    memcpy(ptr, span.data(), span.size());
    ptr += span.size();
  }

  return Create(std::move(contents));
}

DOMArrayBuffer* DOMArrayBuffer::Slice(size_t begin, size_t end) const {
  begin = std::min(begin, ByteLength());
  end = std::min(end, ByteLength());
  size_t size = begin <= end ? end - begin : 0;
  return Create(static_cast<const char*>(Data()) + begin, size);
}

}  // namespace blink
