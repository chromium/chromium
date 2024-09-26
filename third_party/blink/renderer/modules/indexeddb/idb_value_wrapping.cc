// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_binding_for_modules.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/snappy/src/snappy.h"

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
static const uint8_t kRequiresProcessingSSVPseudoVersion = 17;

// SSV processing command replacing the SSV data bytes with a Blob's contents.
//
// 1) 0xFF - kVersionTag
// 2) 0x11 - kRequiresProcessingSSVPseudoVersion
// 3) 0x01 - kReplaceWithBlob
// 4) varint - Blob size
// 5) varint - the offset of the SSV-wrapping Blob in the IDBValue list of Blobs
//             (should always be the last Blob)
static const uint8_t kReplaceWithBlob = 1;

// A similar approach is used to notate compressed data.
// 1) 0xFF - kVersionTag
// 2) 0x11 - kRequiresProcessingSSVPseudoVersion
// 3) 0x02 - kCompressedWithSnappy
// 4) the compressed data

// Data is compressed using Snappy in a single chunk (i.e. without framing).
static const uint8_t kCompressedWithSnappy = 2;

// The number of header bytes in the above scheme.
static const size_t kHeaderSize = 3u;

// Evaluates whether to transmit and store a payload in its compressed form
// based on the compression achieved. Decompressing has a cost in terms of both
// CPU and memory usage, so we skip it for less compressible or jumbo data.
bool ShouldTransmitCompressed(size_t uncompressed_length,
                              size_t compressed_length) {
  // Don't keep compressed if compression ratio is poor.
  if (compressed_length > uncompressed_length * 0.9) {
    return false;
  }

  // Don't keep compressed if decompressed size is large. Snappy doesn't have
  // native support for streamed decoding, so decompressing requires
  // O(uncompressed_length) memory more than handling an uncompressed value
  // would.
  // TODO(estade): implement framing as described in
  // https://github.com/google/snappy/blob/main/framing_format.txt
  if (compressed_length > 256000U) {
    return false;
  }

  return true;
}

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

  *clone = DeserializeScriptValue(script_state, serialized_value_.get(),
                                  &blob_info_);
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

void IDBValueWrapper::DoneCloning() {
#if DCHECK_IS_ON()
  DCHECK(!had_exception_) << __func__
                          << " called on wrapper with serialization exception";
  DCHECK(!done_cloning_) << __func__ << " called twice";
  done_cloning_ = true;
  DCHECK(owns_blob_handles_)
      << __func__ << " called after TakeBlobDataHandles()";
  DCHECK(owns_blob_info_) << __func__ << " called after TakeBlobInfo()";
  DCHECK(owns_wire_bytes_) << __func__ << " called after TakeWireBytes()";
#endif  // DCHECK_IS_ON()

  for (const auto& kvp : serialized_value_->BlobDataHandles()) {
    blob_handles_.push_back(std::move(kvp.value));
  }

  wire_data_ = serialized_value_->GetWireData();
  MaybeCompress();
  MaybeStoreInBlob();
}

bool IDBValueWrapper::ShouldCompress(size_t uncompressed_length) const {
  return uncompressed_length >= compression_threshold_override_.value_or(
                                    mojom::blink::kIDBWrapThreshold);
}

void IDBValueWrapper::MaybeCompress() {

  DCHECK(wire_data_buffer_.empty());
  const size_t wire_data_size = wire_data_.size();

  if (!ShouldCompress(wire_data_size)) {
    return;
  }

  wire_data_buffer_.resize(
      kHeaderSize +
      static_cast<wtf_size_t>(snappy::MaxCompressedLength(wire_data_size)));
  wire_data_buffer_[0] = static_cast<uint8_t>(kVersionTag);
  wire_data_buffer_[1] = kRequiresProcessingSSVPseudoVersion;
  wire_data_buffer_[2] = kCompressedWithSnappy;
  size_t compressed_length;
  snappy::RawCompress(
      reinterpret_cast<const char*>(wire_data_.data()), wire_data_size,
      reinterpret_cast<char*>(wire_data_buffer_.data() + kHeaderSize),
      &compressed_length);
  if (ShouldTransmitCompressed(wire_data_size, compressed_length)) {
    // Truncate the excess space that was previously allocated.
    wire_data_buffer_.resize(kHeaderSize +
                             static_cast<wtf_size_t>(compressed_length));
  } else {
    CHECK_GE(wire_data_buffer_.size(), wire_data_size);
    // Compression wasn't very successful, but we still allocated a large chunk
    // of memory, so we can repurpose it. This copy saves us from making another
    // allocation later on in `MaybeStoreInBlob()` or `TakeWireBytes()`.
    memcpy(wire_data_buffer_.data(), wire_data_.data(), wire_data_size);
    wire_data_buffer_.resize(static_cast<wtf_size_t>(wire_data_size));
  }

  wire_data_ = base::make_span(
      reinterpret_cast<const uint8_t*>(wire_data_buffer_.data()),
      wire_data_buffer_.size());
}

void IDBValueWrapper::MaybeStoreInBlob() {
  const unsigned wrapping_threshold =
      wrapping_threshold_override_.value_or(mojom::blink::kIDBWrapThreshold);
  if (wire_data_.size() <= wrapping_threshold) {
    return;
  }

  // TODO(pwnall): The MIME type should probably be an atomic string.
  String mime_type(kWrapMimeType);
  auto wrapper_blob_data = std::make_unique<BlobData>();
  wrapper_blob_data->SetContentType(String(kWrapMimeType));

  if (wire_data_buffer_.empty()) {
    DCHECK(!ShouldCompress(wire_data_.size()));
    wrapper_blob_data->AppendBytes(wire_data_);
  } else {
    scoped_refptr<RawData> raw_data = RawData::Create();
    raw_data->MutableData()->swap(wire_data_buffer_);
    wrapper_blob_data->AppendData(std::move(raw_data));
  }
  const size_t wire_data_size = wire_data_.size();
  scoped_refptr<BlobDataHandle> wrapper_handle =
      BlobDataHandle::Create(std::move(wrapper_blob_data), wire_data_size);
  blob_info_.emplace_back(wrapper_handle);
  blob_handles_.push_back(std::move(wrapper_handle));

  DCHECK(wire_data_buffer_.empty());
  wire_data_buffer_.push_back(kVersionTag);
  wire_data_buffer_.push_back(kRequiresProcessingSSVPseudoVersion);
  wire_data_buffer_.push_back(kReplaceWithBlob);
  IDBValueWrapper::WriteVarInt(base::checked_cast<unsigned>(wire_data_size),
                               wire_data_buffer_);
  IDBValueWrapper::WriteVarInt(serialized_value_->BlobDataHandles().size(),
                               wire_data_buffer_);

  wire_data_ = base::make_span(
      reinterpret_cast<const uint8_t*>(wire_data_buffer_.data()),
      wire_data_buffer_.size());
  DCHECK(!wire_data_buffer_.empty());
}

Vector<char> IDBValueWrapper::TakeWireBytes() {
#if DCHECK_IS_ON()
  DCHECK(done_cloning_) << __func__ << " called before DoneCloning()";
  DCHECK(owns_wire_bytes_) << __func__ << " called twice";
  owns_wire_bytes_ = false;
#endif  // DCHECK_IS_ON()

  if (wire_data_buffer_.empty()) {
    DCHECK(!ShouldCompress(wire_data_.size()));
    // The wire bytes are coming directly from the SSV's GetWireData() call.
    DCHECK_EQ(wire_data_.data(), serialized_value_->GetWireData().data());
    DCHECK_EQ(wire_data_.size(), serialized_value_->GetWireData().size());
    return Vector<char>(wire_data_);
  }

  // The wire bytes are coming from wire_data_buffer_, so we can avoid a copy.
  DCHECK_EQ(wire_data_buffer_.data(),
            reinterpret_cast<const char*>(wire_data_.data()));
  DCHECK_EQ(wire_data_buffer_.size(), wire_data_.size());
  return std::move(wire_data_buffer_);
}

IDBValueUnwrapper::IDBValueUnwrapper() {
  Reset();
}

// static
bool IDBValueUnwrapper::IsWrapped(IDBValue* value) {
  DCHECK(value);

  if (value->DataSize() < kHeaderSize) {
    return false;
  }
  base::span<const uint8_t> data_span = base::as_byte_span(value->Data());
  return data_span[0] == kVersionTag &&
         data_span[1] == kRequiresProcessingSSVPseudoVersion &&
         data_span[2] == kReplaceWithBlob;
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
void IDBValueUnwrapper::Unwrap(Vector<char>&& wrapper_blob_content,
                               IDBValue& wrapped_value) {
  wrapped_value.SetData(std::move(wrapper_blob_content));
  wrapped_value.TakeLastBlob();
}

// static
bool IDBValueUnwrapper::Decompress(const Vector<char>& buffer,
                                   Vector<char>* out_buffer) {
  if (buffer.size() < kHeaderSize) {
    return false;
  }
  base::span<const uint8_t> data_span = base::as_byte_span(buffer);

  if (data_span[0] != kVersionTag ||
      data_span[1] != kRequiresProcessingSSVPseudoVersion ||
      data_span[2] != kCompressedWithSnappy) {
    return false;
  }

  base::span<const char> compressed(
      base::as_chars(data_span.subspan(kHeaderSize)));

  Vector<char> decompressed_data;
  size_t decompressed_length;
  if (!snappy::GetUncompressedLength(compressed.data(), compressed.size(),
                                     &decompressed_length)) {
    return false;
  }

  decompressed_data.resize(static_cast<wtf_size_t>(decompressed_length));
  snappy::RawUncompress(compressed.data(), compressed.size(),
                        decompressed_data.data());
  *out_buffer = std::move(decompressed_data);
  return true;
}

bool IDBValueUnwrapper::Parse(IDBValue* value) {
  // Fast path that avoids unnecessary dynamic allocations.
  if (!IDBValueUnwrapper::IsWrapped(value))
    return false;

  const uint8_t* data = reinterpret_cast<const uint8_t*>(value->Data().data());
  end_ = data + value->DataSize();
  current_ = data + kHeaderSize;

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
