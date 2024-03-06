// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

#include <utility>

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
                                  const uint8_t* data,
                                  wtf_size_t size,
                                  uint64_t tag) {
  // Don't allow an ID of 0, it is used internally to indicate errors.
  DCHECK(data_type_id);
  DCHECK(data);

  Vector<uint8_t> vector =
      CachedMetadata::GetSerializedDataHeader(data_type_id, size, tag);
  vector.Append(data, size);
  return vector;
}

}  // namespace

scoped_refptr<CachedMetadata> CachedMetadata::Create(uint32_t data_type_id,
                                                     const uint8_t* data,
                                                     size_t size,
                                                     uint64_t tag) {
  return base::MakeRefCounted<CachedMetadata>(
      data_type_id, data, base::checked_cast<wtf_size_t>(size), tag,
      base::PassKey<CachedMetadata>());
}

scoped_refptr<CachedMetadata> CachedMetadata::CreateFromSerializedData(
    const uint8_t* data,
    size_t size) {
  if (size > std::numeric_limits<wtf_size_t>::max())
    return nullptr;
  Vector<uint8_t> copied_data;
  copied_data.Append(data, static_cast<wtf_size_t>(size));
  return CreateFromSerializedData(std::move(copied_data));
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
    mojo_base::BigBuffer& data) {
  if (!CheckSizeAndMarker(data)) {
    return nullptr;
  }
  return base::MakeRefCounted<CachedMetadata>(std::move(data),
                                              base::PassKey<CachedMetadata>());
}

CachedMetadata::CachedMetadata(Vector<uint8_t> data,
                               base::PassKey<CachedMetadata>)
    : buffer_(std::move(data)) {}

CachedMetadata::CachedMetadata(uint32_t data_type_id,
                               const uint8_t* data,
                               wtf_size_t size,
                               uint64_t tag,
                               base::PassKey<CachedMetadata>)
    : buffer_(GetSerializedData(data_type_id, data, size, tag)) {}

CachedMetadata::CachedMetadata(mojo_base::BigBuffer data,
                               base::PassKey<CachedMetadata>)
    : buffer_(std::move(data)) {}

const uint8_t* CachedMetadata::RawData() const {
  if (absl::holds_alternative<Vector<uint8_t>>(buffer_)) {
    return absl::get<Vector<uint8_t>>(buffer_).data();
  }
  CHECK(absl::holds_alternative<mojo_base::BigBuffer>(buffer_));
  return absl::get<mojo_base::BigBuffer>(buffer_).data();
}

uint32_t CachedMetadata::RawSize() const {
  if (absl::holds_alternative<Vector<uint8_t>>(buffer_)) {
    return absl::get<Vector<uint8_t>>(buffer_).size();
  }
  CHECK(absl::holds_alternative<mojo_base::BigBuffer>(buffer_));
  return base::checked_cast<uint32_t>(
      absl::get<mojo_base::BigBuffer>(buffer_).size());
}

absl::variant<Vector<uint8_t>, mojo_base::BigBuffer>
CachedMetadata::DrainSerializedData() && {
  return std::move(buffer_);
}

}  // namespace blink
