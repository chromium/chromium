// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"

#include <utility>

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

ArrayBufferContents PinSharedArrayBufferContent(
    const AllowSharedBufferSource* buffer_union) {
  ArrayBufferContents result;
  switch (buffer_union->GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferAllowShared();
      if (buffer->IsShared()) {
        buffer->Content()->ShareWith(result);
      }
      break;
    }
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared: {
      auto* view = buffer_union->GetAsArrayBufferViewAllowShared().Get();
      if (view->IsShared()) {
        view->BufferShared()->Content()->ShareWith(result);
      }
      break;
    }
  }
  return result;
}

std::pair<ArrayBufferContents, base::span<uint8_t>> TransferArrayBuffers(
    v8::Isolate* isolate,
    const HeapVector<Member<DOMArrayBuffer>>& transfer_list,
    DOMArrayBufferView* data,
    ExceptionState& exception_state) {
  // Before transferring anything, we check that all the arraybuffers in the
  // list are transferable and there are no duplicates.
  HeapHashSet<Member<DOMArrayBuffer>> seen_buffers;
  for (const Member<DOMArrayBuffer>& array_buffer : transfer_list) {
    if (!array_buffer->IsDetachable(isolate) || array_buffer->IsDetached()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "Cannot detach ArrayBuffer");
      return {};
    }

    if (!seen_buffers.insert(array_buffer).is_new_entry) {
      // While inserting we found that the buffer has already been seen.
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Duplicate ArrayBuffers in the transfer list");
      return {};
    }
  }

  // Transfer all arraybuffers. If one of them backs `data`, transfer it via the
  // view so the returned span tracks the (possibly copied) contents.
  std::pair<ArrayBufferContents, base::span<uint8_t>> result{
      {}, data->ByteSpanMaybeShared()};
  for (const Member<DOMArrayBuffer>& array_buffer : transfer_list) {
    if (array_buffer == data->buffer()) {
      CHECK(!data->IsShared());
      result = TransferArrayBufferView(isolate, data, exception_state);
    } else {
      ArrayBufferContents contents;
      array_buffer->Transfer(isolate, contents, exception_state);
    }
    if (exception_state.HadException()) {
      return {};
    }
  }
  return result;
}

}  // namespace blink
