// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

#include <utility>
#include <variant>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

namespace blink {

namespace {

template <typename DataType>
bool CheckSizeAndMarker(const DataType& data) {
  // Ensure the data is big enough.
  if (data.size() <= sizeof(CachedMetadataHeader)) {
    return false;
  }
  // Ensure the marker matches.
  if (reinterpret_cast<const CachedMetadataHeader*>(data.data())->marker !=
      CachedMetadataHandler::kSingleEntryWithTag) {
    return false;
  }
  return true;
}

Vector<uint8_t> GetSerializedData(uint32_t data_type_id,
                                  base::span<const uint8_t> data,
                                  uint64_t tag) {
  // Don't allow an ID of 0, it is used internally to indicate errors.
  DCHECK(data_type_id);
  DCHECK(data.data());

  Vector<uint8_t> vector =
      CachedMetadata::GetSerializedDataHeader(data_type_id, data.size(), tag);
  vector.AppendSpan(data);
  return vector;
}

}  // namespace

scoped_refptr<CachedMetadata> CachedMetadata::Create(
    uint32_t data_type_id,
    base::span<const uint8_t> data,
    uint64_t tag) {
  return base::MakeRefCounted<CachedMetadata>(data_type_id, data, tag,
                                              base::PassKey<CachedMetadata>());
}

scoped_refptr<CachedMetadata> CachedMetadata::CreateFromSerializedData(
    Vector<uint8_t> data) {
  if (!CheckSizeAndMarker(data)) {
    return nullptr;
  }
  return base::MakeRefCounted<CachedMetadata>(std::move(data),
                                              base::PassKey<CachedMetadata>());
}

scoped_refptr<CachedMetadata> CachedMetadata::CreateFromSerializedData(
    mojo_base::BigBuffer& data,
    uint32_t offset) {
  if (data.size() < offset ||
      !CheckSizeAndMarker(base::as_byte_span(data).subspan(offset))) {
    return nullptr;
  }
  return base::MakeRefCounted<CachedMetadata>(std::move(data), offset,
                                              base::PassKey<CachedMetadata>());
}

CachedMetadata::CachedMetadata(Vector<uint8_t> data,
                               base::PassKey<CachedMetadata>)
    : buffer_(std::move(data)) {}

CachedMetadata::CachedMetadata(uint32_t data_type_id,
                               base::span<const uint8_t> data,
                               uint64_t tag,
                               base::PassKey<CachedMetadata>)
    : buffer_(GetSerializedData(data_type_id, data, tag)) {}

CachedMetadata::CachedMetadata(mojo_base::BigBuffer data,
                               uint32_t offset,
                               base::PassKey<CachedMetadata>)
    : buffer_(std::move(data)), offset_(offset) {}

base::span<const uint8_t> CachedMetadata::SerializedData() const {
  base::span<const uint8_t> span_including_offset;
  if (std::holds_alternative<Vector<uint8_t>>(buffer_)) {
    span_including_offset = std::get<Vector<uint8_t>>(buffer_);
  } else {
    CHECK(std::holds_alternative<mojo_base::BigBuffer>(buffer_));
    span_including_offset = std::get<mojo_base::BigBuffer>(buffer_);
  }
  CHECK_GE(span_including_offset.size(), offset_);
  return span_including_offset.subspan(offset_);
}

std::variant<Vector<uint8_t>, mojo_base::BigBuffer>
CachedMetadata::DrainSerializedData() && {
  return std::move(buffer_);
}

}  // namespace blink
