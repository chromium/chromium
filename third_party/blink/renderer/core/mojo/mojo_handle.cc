// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/mojo_handle.h"

#include "base/numerics/safe_math.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_create_shared_buffer_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_discard_data_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_duplicate_buffer_handle_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_map_buffer_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_read_data_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_read_data_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_read_message_flags.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_read_message_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_write_data_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_write_data_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/mojo/mojo_watcher.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

// Mojo messages typically do not contain many handles. In fact most
// messages do not contain any handle. An inline capacity of 4 should avoid
// heap allocation in vast majority of cases.
static const size_t kHandleVectorInlineCapacity = 4;

namespace blink {

namespace {

base::span<uint8_t> ByteSpanForBufferSource(const V8BufferSource& buffer) {
  switch (buffer.GetContentType()) {
    case V8BufferSource::ContentType::kArrayBuffer:
      return buffer.GetAsArrayBuffer()->ByteSpan();
    case V8BufferSource::ContentType::kArrayBufferView:
      return buffer.GetAsArrayBufferView()->ByteSpan();
  }
  return {};
}

}  // namespace

mojo::ScopedHandle MojoHandle::TakeHandle() {
  return std::move(handle_);
}

MojoHandle::MojoHandle(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

void MojoHandle::close() {
  handle_.reset();
}

MojoWatcher* MojoHandle::watch(ScriptState* script_state,
                               const MojoHandleSignals* signals,
                               V8MojoWatchCallback* callback) {
  return MojoWatcher::Create(handle_.get(), signals, callback,
                             ExecutionContext::From(script_state));
}

MojoResult MojoHandle::writeMessage(
    const V8BufferSource* buffer,
    const HeapVector<Member<MojoHandle>>& handles) {
  Vector<mojo::ScopedHandle, kHandleVectorInlineCapacity> scoped_handles;
  scoped_handles.reserve(handles.size());
  bool has_invalid_handles = false;
  for (auto& handle : handles) {
    if (!handle->handle_.is_valid())
      has_invalid_handles = true;
    else
      scoped_handles.emplace_back(std::move(handle->handle_));
  }
  if (has_invalid_handles)
    return MOJO_RESULT_INVALID_ARGUMENT;

  base::span<const uint8_t> bytes = ByteSpanForBufferSource(*buffer);

  auto message = mojo::Message(bytes, base::make_span(scoped_handles));
  DCHECK(!message.IsNull());
  return mojo::WriteMessageNew(mojo::MessagePipeHandle(handle_.get().value()),
                               message.TakeMojoMessage(),
                               MOJO_WRITE_MESSAGE_FLAG_NONE);
}

MojoReadMessageResult* MojoHandle::readMessage(
    const MojoReadMessageFlags* flags_dict) {
  MojoReadMessageResult* result_dict = MojoReadMessageResult::Create();

  mojo::ScopedMessageHandle message;
  MojoResult result =
      mojo::ReadMessageNew(mojo::MessagePipeHandle(handle_.get().value()),
                           &message, MOJO_READ_MESSAGE_FLAG_NONE);
  if (result != MOJO_RESULT_OK) {
    result_dict->setResult(result);
    return result_dict;
  }

  result = MojoSerializeMessage(message->value(), nullptr);
  if (result != MOJO_RESULT_OK && result != MOJO_RESULT_FAILED_PRECONDITION) {
    result_dict->setResult(MOJO_RESULT_ABORTED);
    return result_dict;
  }

  uint32_t num_bytes = 0, num_handles = 0;
  void* bytes;
  Vector<::MojoHandle, kHandleVectorInlineCapacity> raw_handles;
  result = MojoGetMessageData(message->value(), nullptr, &bytes, &num_bytes,
                              nullptr, &num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    raw_handles.resize(num_handles);
    result = MojoGetMessageData(message->value(), nullptr, &bytes, &num_bytes,
                                raw_handles.data(), &num_handles);
  }

  if (result != MOJO_RESULT_OK) {
    result_dict->setResult(MOJO_RESULT_ABORTED);
    return result_dict;
  }

  DOMArrayBuffer* buffer =
      DOMArrayBuffer::CreateUninitializedOrNull(num_bytes, 1);
  if (num_bytes) {
    CHECK(buffer);
    memcpy(buffer->Data(), bytes, num_bytes);
  }
  result_dict->setBuffer(buffer);

  HeapVector<Member<MojoHandle>> handles(num_handles);
  for (uint32_t i = 0; i < num_handles; ++i) {
    handles[i] = MakeGarbageCollected<MojoHandle>(
        mojo::MakeScopedHandle(mojo::Handle(raw_handles[i])));
  }
  result_dict->setHandles(handles);
  result_dict->setResult(result);

  return result_dict;
}

MojoWriteDataResult* MojoHandle::writeData(
    const V8BufferSource* buffer,
    const MojoWriteDataOptions* options_dict) {
  MojoWriteDataFlags flags = MOJO_WRITE_DATA_FLAG_NONE;
  if (options_dict->allOrNone())
    flags |= MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;

  base::span<const uint8_t> bytes = ByteSpanForBufferSource(*buffer);

  ::MojoWriteDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  uint32_t num_bytes = 0;
  MojoResult result =
      base::CheckedNumeric<uint32_t>(bytes.size()).AssignIfValid(&num_bytes)
          ? MojoWriteData(handle_.get().value(), bytes.data(), &num_bytes,
                          &options)
          : MOJO_RESULT_INVALID_ARGUMENT;

  MojoWriteDataResult* result_dict = MojoWriteDataResult::Create();
  result_dict->setResult(result);
  result_dict->setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
  return result_dict;
}

MojoReadDataResult* MojoHandle::queryData() const {
  MojoReadDataResult* result_dict = MojoReadDataResult::Create();
  uint32_t num_bytes = 0;
  ::MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_READ_DATA_FLAG_QUERY;
  MojoResult result =
      MojoReadData(handle_.get().value(), &options, nullptr, &num_bytes);
  result_dict->setResult(result);
  result_dict->setNumBytes(num_bytes);
  return result_dict;
}

MojoReadDataResult* MojoHandle::discardData(
    unsigned num_bytes,
    const MojoDiscardDataOptions* options_dict) {
  MojoReadDataResult* result_dict = MojoReadDataResult::Create();
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_DISCARD;
  if (options_dict->allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;

  ::MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  MojoResult result =
      MojoReadData(handle_.get().value(), &options, nullptr, &num_bytes);
  result_dict->setResult(result);
  result_dict->setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
  return result_dict;
}

MojoReadDataResult* MojoHandle::readData(
    const V8BufferSource* buffer,
    const MojoReadDataOptions* options_dict) const {
  MojoReadDataFlags flags = MOJO_READ_DATA_FLAG_NONE;
  if (options_dict->allOrNone())
    flags |= MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  if (options_dict->peek())
    flags |= MOJO_READ_DATA_FLAG_PEEK;

  base::span<uint8_t> bytes = ByteSpanForBufferSource(*buffer);

  ::MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;

  uint32_t num_bytes;
  MojoResult result =
      base::CheckedNumeric<uint32_t>(bytes.size()).AssignIfValid(&num_bytes)
          ? MojoReadData(handle_.get().value(), &options, bytes.data(),
                         &num_bytes)
          : MOJO_RESULT_INVALID_ARGUMENT;

  MojoReadDataResult* result_dict = MojoReadDataResult::Create();
  result_dict->setResult(result);
  result_dict->setNumBytes(result == MOJO_RESULT_OK ? num_bytes : 0);
  return result_dict;
}

MojoMapBufferResult* MojoHandle::mapBuffer(unsigned offset,
                                           unsigned num_bytes) {
  MojoMapBufferResult* result_dict = MojoMapBufferResult::Create();

  // We need to extract the underlying shared memory region to map it as array
  // buffer contents. However, as we don't know what kind of shared memory
  // region is currently backing the buffer, we unwrap it to the underlying
  // platform shared memory region. We also don't want to duplicate the region,
  // and so need to perform a small dance here to first unwrap, and later
  // re-wrap the MojoSharedBuffer to/from a //base shared memory region.
  mojo::SharedBufferHandle buffer_handle(handle_.release().value());
  auto region = mojo::UnwrapPlatformSharedMemoryRegion(
      mojo::ScopedSharedBufferHandle(buffer_handle));

  if (region.IsValid()) {
    ArrayBufferContents contents(region, offset, num_bytes);
    if (contents.IsValid()) {
      result_dict->setResult(MOJO_RESULT_OK);
      result_dict->setBuffer(DOMArrayBuffer::Create(contents));
    } else {
      // Contents would be invalid if `MapAt()` failed.
      result_dict->setResult(MOJO_RESULT_INVALID_ARGUMENT);
    }
  } else {
    result_dict->setResult(MOJO_RESULT_UNKNOWN);
  }

  // 2nd part of the dance: we now need to wrap the shared memory region into a
  // mojo handle again.
  mojo::ScopedSharedBufferHandle mojo_buffer =
      mojo::WrapPlatformSharedMemoryRegion(std::move(region));
  handle_.reset(mojo::Handle(mojo_buffer.release().value()));

  return result_dict;
}

MojoCreateSharedBufferResult* MojoHandle::duplicateBufferHandle(
    const MojoDuplicateBufferHandleOptions* options_dict) {
  MojoCreateSharedBufferResult* result_dict =
      MojoCreateSharedBufferResult::Create();

  ::MojoDuplicateBufferHandleOptions options = {
      sizeof(options), MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_NONE};
  if (options_dict->readOnly())
    options.flags |= MOJO_DUPLICATE_BUFFER_HANDLE_FLAG_READ_ONLY;

  mojo::Handle handle;
  MojoResult result = MojoDuplicateBufferHandle(handle_.get().value(), &options,
                                                handle.mutable_value());
  result_dict->setResult(result);
  if (result == MOJO_RESULT_OK) {
    result_dict->setHandle(
        MakeGarbageCollected<MojoHandle>(mojo::MakeScopedHandle(handle)));
  }
  return result_dict;
}

}  // namespace blink
