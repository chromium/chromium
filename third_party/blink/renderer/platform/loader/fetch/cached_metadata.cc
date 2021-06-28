// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"

namespace blink {

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
  // Ensure the data is big enough, otherwise discard the data.
  if (data.size() < kCachedMetaDataStart)
    return nullptr;
  // Ensure the marker matches, otherwise discard the data.
  if (*reinterpret_cast<const uint32_t*>(data.data()) !=
      CachedMetadataHandler::kSingleEntry) {
    return nullptr;
  }
  return base::AdoptRef(new CachedMetadata(std::move(data)));
}

scoped_refptr<CachedMetadata> CachedMetadata::CreateFromSerializedData(
    mojo_base::BigBuffer data) {
  // Ensure the data is big enough, otherwise discard the data.
  if (data.size() < kCachedMetaDataStart)
    return nullptr;
  // Ensure the marker matches, otherwise discard the data.
  if (*reinterpret_cast<const uint32_t*>(data.data()) !=
      CachedMetadataHandler::kSingleEntry) {
    return nullptr;
  }
  return base::AdoptRef(new CachedMetadata(std::move(data)));
}

CachedMetadata::CachedMetadata(Vector<uint8_t> data) {
  // Serialized metadata should have non-empty data.
  DCHECK_GT(data.size(), kCachedMetaDataStart);
  DCHECK(!data.IsEmpty());
  // Make sure that the first int in the data is the single entry marker.
  CHECK_EQ(*reinterpret_cast<const uint32_t*>(data.data()),
           CachedMetadataHandler::kSingleEntry);

  vector_ = std::move(data);
}

CachedMetadata::CachedMetadata(uint32_t data_type_id,
                               const uint8_t* data,
                               wtf_size_t size) {
  // Don't allow an ID of 0, it is used internally to indicate errors.
  DCHECK(data_type_id);
  DCHECK(data);

  vector_.ReserveInitialCapacity(kCachedMetaDataStart + size);
  uint32_t marker = CachedMetadataHandler::kSingleEntry;
  vector_.Append(reinterpret_cast<const uint8_t*>(&marker), sizeof(uint32_t));
  vector_.Append(reinterpret_cast<const uint8_t*>(&data_type_id),
                 sizeof(uint32_t));
  vector_.Append(data, size);
}

CachedMetadata::CachedMetadata(mojo_base::BigBuffer data) {
  // Serialized metadata should have non-empty data.
  DCHECK_GT(data.size(), kCachedMetaDataStart);
  // Make sure that the first int in the data is the single entry marker.
  CHECK_EQ(*reinterpret_cast<const uint32_t*>(data.data()),
           CachedMetadataHandler::kSingleEntry);

  buffer_ = std::move(data);
}

}  // namespace blink
