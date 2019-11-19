// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// V8 values are stored on disk by IndexedDB using the format implemented in
// SerializedScriptValue (SSV). The wrapping detection logic in
// IDBValueUnwrapper::IsWrapped() must be able to distinguish between SSV byte
// sequences produced and byte sequences expressing the fact that an IDBValue
// has been wrapped and requires post-processing.
//
// The detection logic takes advantage of the highly regular structure around
// SerializedScriptValue. A version 17 byte sequence always starts with the
// following four bytes:
//
// 1) 0xFF - kVersionTag
// 2) 0x11 - Blink wrapper version, 17
// 3) 0xFF - kVersionTag
// 4) 0x0D - V8 serialization version, currently 13, doesn't matter
//
// It follows that SSV will never produce byte sequences starting with 0xFF,
// 0x11, and any value except for 0xFF. If the SSV format changes, the version
// will have to be bumped.

// The SSV format version whose encoding hole is (ab)used for wrapping.
const static uint8_t kRequiresProcessingSSVPseudoVersion = 17;

// SSV processing command replacing the SSV data bytes with a Blob's contents.
//
// 1) 0xFF - kVersionTag
// 2) 0x11 - kRequiresProcessingSSVPseudoVersion
// 3) 0x01 - kReplaceWithBlob
// 4) varint - Blob size
// 5) varint - the offset of the SSV-wrapping Blob in the IDBValue list of Blobs
//             (should always be the last Blob)
const static uint8_t kReplaceWithBlob = 1;

}  // namespace

IDBValueWrapper::IDBValueWrapper(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    SerializedScriptValue::SerializeOptions::WasmSerializationPolicy
        wasm_policy,
    ExceptionState& exception_state) {
  SerializedScriptValue::SerializeOptions options;
  options.blob_info = &blob_info_;
  options.for_storage = SerializedScriptValue::kForStorage;
  options.wasm_policy = wasm_policy;

  serialized_value_ = SerializedScriptValue::Serialize(isolate, value, options,
                                                       exception_state);
  if (serialized_value_) {
    original_data_length_ = serialized_value_->DataLengthInBytes();
  }
#if DCHECK_IS_ON()
  if (exception_state.HadException())
    had_exception_ = true;
#endif  // DCHECK_IS_ON()
}

// Explicit destructor in the .cpp file, to move the dependency on the
// BlobDataHandle definition away from the header file.
IDBValueWrapper::~IDBValueWrapper() = default;

void IDBValueWrapper::Clone(ScriptState* script_state, ScriptValue* clone) {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __func__
                          << " called on wrapper with serialization exception";
  DCHECK(!done_cloning_) << __func__ << " called after DoneCloning()";
#endif  // DCHECK_IS_ON()

  bool read_wasm_from_stream = true;
  // It is safe to unconditionally enable WASM module decoding because the
  // relevant checks were already performed in SerializedScriptValue::Serialize,
  // called by the IDBValueWrapper constructor.
  *clone = DeserializeScriptValue(script_state, serialized_value_.get(),
                                  &blob_info_, read_wasm_from_stream);
}

// static
void IDBValueWrapper::WriteVarInt(unsigned value, Vector<char>& output) {
  // Writes an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to
  // the most significant 7 bits. Each byte, except the last, has the MSB set.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  do {
    output.push_back((value & 0x7F) | 0x80);
    value >>= 7;
  } while (value);
  output.back() &= 0x7F;
}

// static
void IDBValueWrapper::WriteBytes(const Vector<uint8_t>& bytes,
                                 Vector<char>& output) {
  IDBValueWrapper::WriteVarInt(bytes.size(), output);
  output.Append(bytes.data(), bytes.size());
}

void IDBValueWrapper::DoneCloning() {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __func__
                          << " called on wrapper with serialization exception";
  DCHECK(!done_cloning_) << __func__ << " called twice";
  done_cloning_ = true;
#endif  // DCHECK_IS_ON()

  wire_data_ = serialized_value_->GetWireData();
  for (const auto& kvp : serialized_value_->BlobDataHandles())
    blob_handles_.push_back(std::move(kvp.value));
}

bool IDBValueWrapper::WrapIfBiggerThan(unsigned max_bytes) {
#if DCHECK_IS_ON()
  DCHECK(done_cloning_) << __func__ << " called before DoneCloning()";
  DCHECK(owns_blob_handles_)
      << __func__ << " called after TakeBlobDataHandles()";
  DCHECK(owns_blob_info_) << __func__ << " called after TakeBlobInfo()";
  DCHECK(owns_wire_bytes_) << __func__ << " called after TakeWireBytes()";
#endif  // DCHECK_IS_ON()

  size_t wire_data_size = wire_data_.size();
  if (wire_data_size <= max_bytes)
    return false;

  // TODO(pwnall): The MIME type should probably be an atomic string.
  String mime_type(kWrapMimeType);
  auto wrapper_blob_data = std::make_unique<BlobData>();
  wrapper_blob_data->SetContentType(String(kWrapMimeType));
  wrapper_blob_data->AppendBytes(wire_data_.data(), wire_data_size);
  scoped_refptr<BlobDataHandle> wrapper_handle =
      BlobDataHandle::Create(std::move(wrapper_blob_data), wire_data_size);
  blob_info_.emplace_back(wrapper_handle);
  blob_handles_.push_back(std::move(wrapper_handle));

  wire_data_buffer_.clear();
  wire_data_buffer_.push_back(kVersionTag);
  wire_data_buffer_.push_back(kRequiresProcessingSSVPseudoVersion);
  wire_data_buffer_.push_back(kReplaceWithBlob);
  IDBValueWrapper::WriteVarInt(SafeCast<unsigned>(wire_data_size),
                               wire_data_buffer_);
  IDBValueWrapper::WriteVarInt(serialized_value_->BlobDataHandles().size(),
                               wire_data_buffer_);

  wire_data_ = base::make_span(
      reinterpret_cast<const uint8_t*>(wire_data_buffer_.data()),
      wire_data_buffer_.size());
  DCHECK(!wire_data_buffer_.IsEmpty());
  return true;
}

scoped_refptr<SharedBuffer> IDBValueWrapper::TakeWireBytes() {
#if DCHECK_IS_ON()
  DCHECK(done_cloning_) << __func__ << " called before DoneCloning()";
  DCHECK(owns_wire_bytes_) << __func__ << " called twice";
  owns_wire_bytes_ = false;
#endif  // DCHECK_IS_ON()

  if (wire_data_buffer_.IsEmpty()) {
    // The wire bytes are coming directly from the SSV's GetWireData() call.
    DCHECK_EQ(wire_data_.data(), serialized_value_->GetWireData().data());
    DCHECK_EQ(wire_data_.size(), serialized_value_->GetWireData().size());
    return SharedBuffer::Create(wire_data_.data(), wire_data_.size());
  }

  // The wire bytes are coming from wire_data_buffer_, so we can avoid a copy.
  DCHECK_EQ(wire_data_buffer_.data(),
            reinterpret_cast<const char*>(wire_data_.data()));
  DCHECK_EQ(wire_data_buffer_.size(), wire_data_.size());
  return SharedBuffer::AdoptVector(wire_data_buffer_);
}

IDBValueUnwrapper::IDBValueUnwrapper() {
  Reset();
}

// static
bool IDBValueUnwrapper::IsWrapped(IDBValue* value) {
  DCHECK(value);

  uint8_t header[3];
  if (!value->data_ || !value->data_->GetBytes(header, sizeof(header)))
    return false;

  return header[0] == kVersionTag &&
         header[1] == kRequiresProcessingSSVPseudoVersion &&
         header[2] == kReplaceWithBlob;
}

// static
bool IDBValueUnwrapper::IsWrapped(
    const Vector<std::unique_ptr<IDBValue>>& values) {
  for (const auto& value : values) {
    if (IsWrapped(value.get()))
      return true;
  }
  return false;
}

// static
void IDBValueUnwrapper::Unwrap(
    scoped_refptr<SharedBuffer>&& wrapper_blob_content,
    IDBValue* wrapped_value) {
  DCHECK(wrapped_value);
  DCHECK(wrapped_value->data_);

  wrapped_value->SetData(wrapper_blob_content);
  wrapped_value->TakeLastBlob();
}

bool IDBValueUnwrapper::Parse(IDBValue* value) {
  // Fast path that avoids unnecessary dynamic allocations.
  if (!IDBValueUnwrapper::IsWrapped(value))
    return false;

  const uint8_t* data = reinterpret_cast<const uint8_t*>(value->data_->Data());
  end_ = data + value->data_->size();
  current_ = data + 3;

  if (!ReadVarInt(blob_size_))
    return Reset();

  unsigned blob_offset;
  if (!ReadVarInt(blob_offset))
    return Reset();

  size_t value_blob_count = value->blob_info_.size();
  if (!value_blob_count || blob_offset != value_blob_count - 1)
    return Reset();

  blob_handle_ = value->blob_info_.back().GetBlobHandle();
  if (blob_handle_->size() != blob_size_)
    return Reset();

  return true;
}

scoped_refptr<BlobDataHandle> IDBValueUnwrapper::WrapperBlobHandle() {
  DCHECK(blob_handle_);

  return std::move(blob_handle_);
}

bool IDBValueUnwrapper::ReadVarInt(unsigned& value) {
  value = 0;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (current_ >= end_)
      return false;

    if (shift >= sizeof(unsigned) * 8)
      return false;
    uint8_t byte = *current_;
    ++current_;
    value |= static_cast<unsigned>(byte & 0x7F) << shift;
    shift += 7;

    has_another_byte = byte & 0x80;
  } while (has_another_byte);
  return true;
}

bool IDBValueUnwrapper::ReadBytes(Vector<uint8_t>& value) {
  unsigned length;
  if (!ReadVarInt(length))
    return false;

  DCHECK_LE(current_, end_);
  if (end_ - current_ < static_cast<ptrdiff_t>(length))
    return false;
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(length);
  result.Append(current_, length);
  value = std::move(result);
  current_ += length;
  return true;
}

bool IDBValueUnwrapper::Reset() {
#if DCHECK_IS_ON()
  blob_handle_ = nullptr;
  current_ = nullptr;
  end_ = nullptr;
#endif  // DCHECK_IS_ON()
  return false;
}

}  // namespace blink
