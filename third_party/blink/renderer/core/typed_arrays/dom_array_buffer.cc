// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

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

  if (IsDetached()) {
    result.Detach();
    return false;
  }

  if (!Content()->Data()) {
    // We transfer an empty ArrayBuffer, we can just allocate an empty content.
    result = ArrayBufferContents(Content()->BackingStore());
  } else {
    Content()->Transfer(result);
    Detach();
  }

  Vector<v8::Local<v8::ArrayBuffer>, 4> buffer_handles;
  v8::HandleScope handle_scope(isolate);
  AccumulateArrayBuffersForAllWorlds(isolate, to_transfer, buffer_handles);

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

v8::Local<v8::Value> DOMArrayBuffer::Wrap(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context) {
  DCHECK(!DOMDataStore::ContainsWrapper(this, isolate));

  const WrapperTypeInfo* wrapper_type_info = this->GetWrapperTypeInfo();

  v8::Local<v8::ArrayBuffer> wrapper;
  {
    v8::Context::Scope context_scope(creation_context->CreationContext());
    wrapper = v8::ArrayBuffer::New(isolate, Content()->BackingStore());

    wrapper->Externalize(Content()->BackingStore());
  }

  return AssociateWithWrapper(isolate, wrapper_type_info, wrapper);
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
