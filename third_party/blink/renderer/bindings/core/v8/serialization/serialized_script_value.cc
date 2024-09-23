/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

#include <memory>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected_macros.h"
#include "third_party/blink/public/web/web_serialized_script_value_version.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/transferables.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

SerializedScriptValue::CanDeserializeInCallback& GetCanDeserializeInCallback() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      SerializedScriptValue::CanDeserializeInCallback, g_callback, ());
  return g_callback;
}

}  // namespace

scoped_refptr<SerializedScriptValue> SerializedScriptValue::Serialize(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializeOptions& options,
    ExceptionState& exception) {
  return SerializedScriptValueFactory::Instance().Create(isolate, value,
                                                         options, exception);
}

scoped_refptr<SerializedScriptValue>
SerializedScriptValue::SerializeAndSwallowExceptions(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  DummyExceptionStateForTesting exception_state;
  scoped_refptr<SerializedScriptValue> serialized =
      Serialize(isolate, value, SerializeOptions(), exception_state);
  if (exception_state.HadException())
    return NullValue();
  return serialized;
}

scoped_refptr<SerializedScriptValue> SerializedScriptValue::Create() {
  return base::AdoptRef(new SerializedScriptValue);
}

scoped_refptr<SerializedScriptValue> SerializedScriptValue::Create(
    const String& data) {
  base::CheckedNumeric<size_t> data_buffer_size = data.length();
  data_buffer_size *= 2;
  if (!data_buffer_size.IsValid())
    return Create();

  DataBufferPtr data_buffer = AllocateBuffer(data_buffer_size.ValueOrDie());
  // TODO(danakj): This cast is valid, since it's at the start of the allocation
  // which will be aligned correctly for UChar. However the pattern of casting
  // byte pointers to other types is problematic and can cause UB. String should
  // provide a way to copy directly to a byte array without forcing the caller
  // to do this case.
  data.CopyTo(reinterpret_cast<UChar*>(data_buffer.data()), 0, data.length());

  return base::AdoptRef(new SerializedScriptValue(std::move(data_buffer)));
}

// Returns whether `tag` was a valid tag in the v0 serialization format.
inline static constexpr bool IsV0VersionTag(uint8_t tag) {
  // There were 13 tags supported in version 0:
  //
  //  35 - 0x23 - # - ImageDataTag
  //  64 - 0x40 - @ - SparseArrayTag
  //  68 - 0x44 - D - DateTag
  //  73 - 0x49 - I - Int32Tag
  //  78 - 0x4E - N - NumberTag
  //  82 - 0x52 - R - RegExpTag
  //  83 - 0x53 - S - StringTag
  //  85 - 0x55 - U - Uint32Tag
  //  91 - 0x5B - [ - ArrayTag
  //  98 - 0x62 - b - BlobTag
  // 102 - 0x66 - f - FileTag
  // 108 - 0x6C - l - FileListTag
  // 123 - 0x7B - { - ObjectTag
  return tag == 35 || tag == 64 || tag == 68 || tag == 73 || tag == 78 ||
         tag == 82 || tag == 83 || tag == 85 || tag == 91 || tag == 98 ||
         tag == 102 || tag == 108 || tag == 123;
}

// Versions 16 and below (prior to April 2017) used ntohs() to byte-swap SSV
// data when converting it to the wire format. This was a historical accient.
//
// As IndexedDB stores SSVs to disk indefinitely, we still need to keep around
// the code needed to deserialize the old format.
inline static bool IsByteSwappedWiredData(base::span<const uint8_t> data) {
  // TODO(pwnall): Return false early if we're on big-endian hardware. Chromium
  // doesn't currently support big-endian hardware, and there's no header
  // exposing endianness to Blink yet. ARCH_CPU_LITTLE_ENDIAN seems promising,
  // but Blink is not currently allowed to include files from build/.

  // The first SSV version without byte-swapping has two envelopes (Blink, V8),
  // each of which is at least 2 bytes long.
  if (data.size() < 4u) {
    return true;
  }

  // This code handles the following cases:
  //
  // v0 (byte-swapped)    - [d,    t,    ...], t = tag byte, d = first data byte
  // v1-16 (byte-swapped) - [v,    0xFF, ...], v = version (1 <= v <= 16)
  // v17+                 - [0xFF, v,    ...], v = first byte of version varint

  if (data[0] == kVersionTag) {
    // The only case where byte-swapped data can have 0xFF in byte zero is
    // version 0. This can only happen if byte one is a tag (supported in
    // version 0) that takes in extra data, and the first byte of extra data is
    // 0xFF. These tags cannot be used as version numbers in the Blink-side SSV
    // envelope.
    //
    // Why we care about version 0:
    //
    // IndexedDB stores values using the SSV format. Currently, IndexedDB does
    // not do any sort of migration, so a value written with a SSV version will
    // be stored with that version until it is removed via an update or delete.
    //
    // IndexedDB was shipped in Chrome 11, which was released on April 27, 2011.
    // SSV version 1 was added in WebKit r91698, which was shipped in Chrome 14,
    // which was released on September 16, 2011.
    static_assert(
        !IsV0VersionTag(SerializedScriptValue::kWireFormatVersion),
        "Using a burned version will prevent us from reading SSV version 0");
    // TODO(pwnall): Add UMA metric here.
    return IsV0VersionTag(data[1]);
  }

  if (data[1] == kVersionTag) {
    // The last SSV format that used byte-swapping was version 16. The version
    // number is stored (before byte-swapping) after a serialization tag, which
    // is 0xFF.
    return data[0] != kVersionTag;
  }

  // If kVersionTag isn't in any of the first two bytes, this is SSV version 0,
  // which was byte-swapped.
  return true;
}

static void SwapWiredDataIfNeeded(base::span<uint8_t> buffer) {
  if (buffer.size() % sizeof(UChar)) {
    return;
  }

  if (!IsByteSwappedWiredData(buffer)) {
    return;
  }

  static_assert(sizeof(UChar) == 2u);
  for (size_t i = 0u; i < buffer.size(); i += 2u) {
    std::swap(buffer[i], buffer[i + 1u]);
  }
}

scoped_refptr<SerializedScriptValue> SerializedScriptValue::Create(
    base::span<const uint8_t> data) {
  if (data.empty())
    return Create();

  DataBufferPtr data_buffer = AllocateBuffer(data.size());
  data_buffer.as_span().copy_from(data);
  SwapWiredDataIfNeeded(data_buffer.as_span());

  return base::AdoptRef(new SerializedScriptValue(std::move(data_buffer)));
}

SerializedScriptValue::SerializedScriptValue()
    : has_registered_external_allocation_(false) {}

SerializedScriptValue::SerializedScriptValue(DataBufferPtr data)
    : data_buffer_(std::move(data)),
      has_registered_external_allocation_(false) {}

void SerializedScriptValue::SetImageBitmapContentsArray(
    ImageBitmapContentsArray contents) {
  image_bitmap_contents_array_ = std::move(contents);
}

SerializedScriptValue::DataBufferPtr SerializedScriptValue::AllocateBuffer(
    size_t buffer_size) {
  // SAFETY: BufferMalloc() always returns a pointer to at least
  // `buffer_size` bytes.
  return UNSAFE_BUFFERS(DataBufferPtr::FromOwningPointer(
      static_cast<uint8_t*>(WTF::Partitions::BufferMalloc(
          buffer_size, "SerializedScriptValue buffer")),
      buffer_size));
}

SerializedScriptValue::~SerializedScriptValue() {
  // If the allocated memory was not registered before, then this class is
  // likely used in a context other than Worker's onmessage environment and the
  // presence of current v8 context is not guaranteed. Avoid calling v8 then.
  if (has_registered_external_allocation_) {
    DCHECK_NE(isolate_, nullptr);
    external_memory_accounter_.Decrease(isolate_.get(), DataLengthInBytes());
  }
}

scoped_refptr<SerializedScriptValue> SerializedScriptValue::NullValue() {
  // The format here may fall a bit out of date, because we support
  // deserializing SSVs written by old browser versions.
  static const uint8_t kNullData[] = {0xFF, 17, 0xFF, 13, '0', 0x00};
  return Create(kNullData);
}

scoped_refptr<SerializedScriptValue> SerializedScriptValue::UndefinedValue() {
  // The format here may fall a bit out of date, because we support
  // deserializing SSVs written by old browser versions.
  static const uint8_t kUndefinedData[] = {0xFF, 17, 0xFF, 13, '_', 0x00};
  return Create(kUndefinedData);
}

String SerializedScriptValue::ToWireString() const {
  // Add the padding '\0', but don't put it in |data_buffer_|.
  // This requires direct use of uninitialized strings, though.
  auto string_size_bytes = base::checked_cast<wtf_size_t>(
      base::bits::AlignUp(data_buffer_.size(), sizeof(UChar)));
  base::span<UChar> backing;
  String wire_string =
      String::CreateUninitialized(string_size_bytes / sizeof(UChar), backing);
  auto [content, padding] =
      base::as_writable_bytes(backing).split_at(data_buffer_.size());
  content.copy_from(data_buffer_);
  if (!padding.empty()) {
    CHECK_EQ(padding.size(), 1u);
    padding[0u] = '\0';
  }
  return wire_string;
}

SerializedScriptValue::ImageBitmapContentsArray
SerializedScriptValue::TransferImageBitmapContents(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  ImageBitmapContentsArray contents;

  if (!image_bitmaps.size())
    return contents;

  for (wtf_size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (image_bitmaps[i]->IsNeutered()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "ImageBitmap at index " +
                                            String::Number(i) +
                                            " is already detached.");
      return contents;
    }
  }

  HeapHashSet<Member<ImageBitmap>> visited;
  for (wtf_size_t i = 0; i < image_bitmaps.size(); ++i) {
    if (visited.Contains(image_bitmaps[i]))
      continue;
    visited.insert(image_bitmaps[i]);
    contents.push_back(image_bitmaps[i]->Transfer());
  }
  return contents;
}

void SerializedScriptValue::TransferImageBitmaps(
    v8::Isolate* isolate,
    const ImageBitmapArray& image_bitmaps,
    ExceptionState& exception_state) {
  image_bitmap_contents_array_ =
      TransferImageBitmapContents(isolate, image_bitmaps, exception_state);
}

void SerializedScriptValue::TransferOffscreenCanvas(
    v8::Isolate* isolate,
    const OffscreenCanvasArray& offscreen_canvases,
    ExceptionState& exception_state) {
  if (!offscreen_canvases.size())
    return;

  HeapHashSet<Member<OffscreenCanvas>> visited;
  for (wtf_size_t i = 0; i < offscreen_canvases.size(); i++) {
    if (visited.Contains(offscreen_canvases[i].Get()))
      continue;
    if (offscreen_canvases[i]->IsNeutered()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "OffscreenCanvas at index " +
                                            String::Number(i) +
                                            " is already detached.");
      return;
    }
    if (offscreen_canvases[i]->RenderingContext()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "OffscreenCanvas at index " +
                                            String::Number(i) +
                                            " has an associated context.");
      return;
    }
    visited.insert(offscreen_canvases[i].Get());
    offscreen_canvases[i].Get()->SetNeutered();
    offscreen_canvases[i].Get()->RecordTransfer();
  }
}

void SerializedScriptValue::TransferReadableStreams(
    ScriptState* script_state,
    const ReadableStreamArray& readable_streams,
    ExceptionState& exception_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  for (ReadableStream* readable_stream : readable_streams) {
    TransferReadableStream(script_state, execution_context, readable_stream,
                           exception_state);
    if (exception_state.HadException())
      return;
  }
}

void SerializedScriptValue::TransferReadableStream(
    ScriptState* script_state,
    ExecutionContext* execution_context,
    ReadableStream* readable_stream,
    ExceptionState& exception_state) {
  MessagePort* local_port = AddStreamChannel(execution_context);
  readable_stream->Serialize(script_state, local_port, exception_state);
  if (exception_state.HadException())
    return;
  // The last element is added by the above `AddStreamChannel()` call.
  streams_.back().readable_optimizer =
      readable_stream->TakeTransferringOptimizer();
}

void SerializedScriptValue::TransferWritableStreams(
    ScriptState* script_state,
    const WritableStreamArray& writable_streams,
    ExceptionState& exception_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  for (WritableStream* writable_stream : writable_streams) {
    TransferWritableStream(script_state, execution_context, writable_stream,
                           exception_state);
    if (exception_state.HadException())
      return;
  }
}

void SerializedScriptValue::TransferWritableStream(
    ScriptState* script_state,
    ExecutionContext* execution_context,
    WritableStream* writable_stream,
    ExceptionState& exception_state) {
  MessagePort* local_port = AddStreamChannel(execution_context);
  writable_stream->Serialize(script_state, local_port, exception_state);
  if (exception_state.HadException())
    return;
  // The last element is added by the above `AddStreamChannel()` call.
  streams_.back().writable_optimizer =
      writable_stream->TakeTransferringOptimizer();
}

void SerializedScriptValue::TransferTransformStreams(
    ScriptState* script_state,
    const TransformStreamArray& transform_streams,
    ExceptionState& exception_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  for (TransformStream* transform_stream : transform_streams) {
    TransferReadableStream(script_state, execution_context,
                           transform_stream->Readable(), exception_state);
    if (exception_state.HadException())
      return;
    TransferWritableStream(script_state, execution_context,
                           transform_stream->Writable(), exception_state);
    if (exception_state.HadException())
      return;
  }
}

// Creates an entangled pair of channels. Adds one end to |streams_| as
// a MessagePortChannel, and returns the other end as a MessagePort.
MessagePort* SerializedScriptValue::AddStreamChannel(
    ExecutionContext* execution_context) {
  // Used for both https://streams.spec.whatwg.org/#rs-transfer and
  // https://streams.spec.whatwg.org/#ws-transfer.
  // 2. Let port1 be a new MessagePort in the current Realm.
  // 3. Let port2 be a new MessagePort in the current Realm.
  MessagePortDescriptorPair pipe;
  auto* local_port = MakeGarbageCollected<MessagePort>(*execution_context);

  // 4. Entangle port1 and port2.
  // As these ports are only meant to transfer streams, we don't care about Task
  // Attribution for them, and hence can pass a nullptr as the MessagePort*
  // here.
  local_port->Entangle(pipe.TakePort0(), nullptr);

  // 9. Set dataHolder.[[port]] to ! StructuredSerializeWithTransfer(port2,
  //    « port2 »).
  streams_.push_back(Stream(pipe.TakePort1()));
  return local_port;
}

void SerializedScriptValue::TransferArrayBuffers(
    v8::Isolate* isolate,
    const ArrayBufferArray& array_buffers,
    ExceptionState& exception_state) {
  array_buffer_contents_array_ =
      TransferArrayBufferContents(isolate, array_buffers, exception_state);
}

void SerializedScriptValue::CloneSharedArrayBuffers(
    SharedArrayBufferArray& array_buffers) {
  if (!array_buffers.size())
    return;

  HeapHashSet<Member<DOMArrayBufferBase>> visited;
  shared_array_buffers_contents_.Grow(array_buffers.size());
  wtf_size_t i = 0;
  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMSharedArrayBuffer* shared_array_buffer = *it;
    if (visited.Contains(shared_array_buffer))
      continue;
    visited.insert(shared_array_buffer);
    shared_array_buffer->ShareContentsWith(shared_array_buffers_contents_[i]);
    i++;
  }
}

v8::Local<v8::Value> SerializedScriptValue::Deserialize(
    v8::Isolate* isolate,
    const DeserializeOptions& options) {
  return SerializedScriptValueFactory::Instance().Deserialize(this, isolate,
                                                              options);
}

// static
UnpackedSerializedScriptValue* SerializedScriptValue::Unpack(
    scoped_refptr<SerializedScriptValue> value) {
  if (!value)
    return nullptr;
#if DCHECK_IS_ON()
  DCHECK(!value->was_unpacked_);
  value->was_unpacked_ = true;
#endif
  return MakeGarbageCollected<UnpackedSerializedScriptValue>(std::move(value));
}

bool SerializedScriptValue::HasPackedContents() const {
  return !array_buffer_contents_array_.empty() ||
         !shared_array_buffers_contents_.empty() ||
         !image_bitmap_contents_array_.empty();
}

bool SerializedScriptValue::ExtractTransferables(
    v8::Isolate* isolate,
    const HeapVector<ScriptValue>& object_sequence,
    Transferables& transferables,
    ExceptionState& exception_state) {
  auto& factory = SerializedScriptValueFactory::Instance();
  wtf_size_t i = 0;
  for (const auto& script_value : object_sequence) {
    v8::Local<v8::Value> value = script_value.V8Value();
    // Validation of non-null objects, per HTML5 spec 10.3.3.
    if (IsUndefinedOrNull(value)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataCloneError,
          "Value at index " + String::Number(i) + " is an untransferable " +
              (value->IsUndefined() ? "'undefined'" : "'null'") + " value.");
      return false;
    }
    if (!factory.ExtractTransferable(isolate, value, i, transferables,
                                     exception_state)) {
      if (!exception_state.HadException()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kDataCloneError,
            "Value at index " + String::Number(i) +
                " does not have a transferable type.");
      }
      return false;
    }
    i++;
  }
  return true;
}

ArrayBufferArray SerializedScriptValue::ExtractNonSharedArrayBuffers(
    Transferables& transferables) {
  ArrayBufferArray& array_buffers = transferables.array_buffers;
  ArrayBufferArray result;
  // Partition array_buffers into [shared..., non_shared...], maintaining
  // relative ordering of elements with the same predicate value.
  auto non_shared_begin =
      std::stable_partition(array_buffers.begin(), array_buffers.end(),
                            [](Member<DOMArrayBufferBase>& array_buffer) {
                              return array_buffer->IsShared();
                            });
  // Copy the non-shared array buffers into result, and remove them from
  // array_buffers.
  result.AppendRange(non_shared_begin, array_buffers.end());
  array_buffers.EraseAt(
      static_cast<wtf_size_t>(non_shared_begin - array_buffers.begin()),
      static_cast<wtf_size_t>(array_buffers.end() - non_shared_begin));
  return result;
}

SerializedScriptValue::ArrayBufferContentsArray
SerializedScriptValue::TransferArrayBufferContents(
    v8::Isolate* isolate,
    const ArrayBufferArray& array_buffers,
    ExceptionState& exception_state) {
  ArrayBufferContentsArray contents;

  if (!array_buffers.size())
    return ArrayBufferContentsArray();

  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer = *it;
    if (array_buffer->IsDetached()) {
      wtf_size_t index =
          static_cast<wtf_size_t>(std::distance(array_buffers.begin(), it));
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "ArrayBuffer at index " +
                                            String::Number(index) +
                                            " is already detached.");
      return ArrayBufferContentsArray();
    }
  }

  contents.Grow(array_buffers.size());
  HeapHashSet<Member<DOMArrayBufferBase>> visited;
  // The scope object to promptly free the backing store to avoid memory
  // regressions.
  // TODO(bikineev): Revisit after young generation is there.
  struct PromptlyFreeSet {
    // The void* is to avoid blink-gc-plugin error.
    void* buffer;
    ~PromptlyFreeSet() {
      static_cast<HeapHashSet<Member<DOMArrayBufferBase>>*>(buffer)->clear();
    }
  } promptly_free_array_buffers{&visited};
  for (auto it = array_buffers.begin(); it != array_buffers.end(); ++it) {
    DOMArrayBufferBase* array_buffer_base = *it;
    if (visited.Contains(array_buffer_base))
      continue;
    visited.insert(array_buffer_base);

    wtf_size_t index =
        static_cast<wtf_size_t>(std::distance(array_buffers.begin(), it));
    if (array_buffer_base->IsShared()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                        "SharedArrayBuffer at index " +
                                            String::Number(index) +
                                            " is not transferable.");
      return ArrayBufferContentsArray();
    } else {
      DOMArrayBuffer* array_buffer =
          static_cast<DOMArrayBuffer*>(array_buffer_base);

      if (!array_buffer->IsDetachable(isolate)) {
        exception_state.ThrowTypeError(
            "ArrayBuffer at index " + String::Number(index) +
            " is not detachable and could not be transferred.");
        return ArrayBufferContentsArray();
      } else if (array_buffer->IsDetached()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                          "ArrayBuffer at index " +
                                              String::Number(index) +
                                              " could not be transferred.");
        return ArrayBufferContentsArray();
      } else if (!array_buffer->Transfer(isolate, contents.at(index),
                                         exception_state)) {
        return ArrayBufferContentsArray();
      }
    }
  }
  return contents;
}

void SerializedScriptValue::
    UnregisterMemoryAllocatedWithCurrentScriptContext() {
  if (has_registered_external_allocation_) {
    DCHECK_NE(isolate_, nullptr);
    external_memory_accounter_.Decrease(isolate_.get(), DataLengthInBytes());
    has_registered_external_allocation_ = false;
  }
}

void SerializedScriptValue::RegisterMemoryAllocatedWithCurrentScriptContext() {
  if (has_registered_external_allocation_)
    return;
  DCHECK_EQ(isolate_, nullptr);
  DCHECK_NE(v8::Isolate::GetCurrent(), nullptr);
  has_registered_external_allocation_ = true;
  isolate_ = v8::Isolate::GetCurrent();
  int64_t diff = static_cast<int64_t>(DataLengthInBytes());
  DCHECK_GE(diff, 0);
  external_memory_accounter_.Increase(isolate_.get(), diff);
}

bool SerializedScriptValue::IsLockedToAgentCluster() const {
  return !wasm_modules_.empty() || !shared_array_buffers_contents_.empty() ||
         base::ranges::any_of(attachments_,
                              [](const auto& entry) {
                                return entry.value->IsLockedToAgentCluster();
                              }) ||
         shared_value_conveyor_.has_value();
}

bool SerializedScriptValue::IsOriginCheckRequired() const {
  return file_system_access_tokens_.size() > 0 || wasm_modules_.size() > 0;
}

bool SerializedScriptValue::CanDeserializeIn(
    ExecutionContext* execution_context) {
  TrailerReader reader(GetWireData());
  RETURN_IF_ERROR(reader.SkipToTrailer(), [](auto) { return false; });
  RETURN_IF_ERROR(reader.Read(), [](auto) { return false; });
  auto& factory = SerializedScriptValueFactory::Instance();
  bool result = base::ranges::all_of(
      reader.required_exposed_interfaces(), [&](SerializationTag tag) {
        return factory.ExecutionContextExposesInterface(execution_context, tag);
      });
  if (const auto& callback = GetCanDeserializeInCallback())
    result = callback.Run(*this, execution_context, result);
  return result;
}

// static
void SerializedScriptValue::OverrideCanDeserializeInForTesting(
    SerializedScriptValue::CanDeserializeInCallback callback) {
  auto& global = GetCanDeserializeInCallback();
  CHECK_NE(callback.is_null(), global.is_null());
  global = std::move(callback);
}

// This ensures that the version number published in
// WebSerializedScriptValueVersion.h matches the serializer's understanding.
// TODO(jbroman): Fix this to also account for the V8-side version. See
// https://crbug.com/704293.
static_assert(kSerializedScriptValueVersion ==
                  SerializedScriptValue::kWireFormatVersion,
              "Update WebSerializedScriptValueVersion.h.");

}  // namespace blink
