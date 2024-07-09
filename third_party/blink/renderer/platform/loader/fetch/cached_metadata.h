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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CACHED_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CACHED_METADATA_H_

#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// |m_serializedData| consists of a 32 bit marker, 32 bits type ID, 64 bits tag,
// and actual data.
struct CachedMetadataHeader {
  uint32_t marker;  // Must be CachedMetadataHandler::kSingleEntryWithTag.
  uint32_t type;
  uint64_t tag;  // This might be 0 if the caller to CachedMetadata::Create did
                 // not specify a value.
};

// Metadata retrieved from the embedding application's cache.
//
// Serialized data is NOT portable across architectures. However, reading the
// data type ID will reject data generated with a different byte-order.
class PLATFORM_EXPORT CachedMetadata : public RefCounted<CachedMetadata> {
  USING_FAST_MALLOC(CachedMetadata);

 public:
  // Returns a Vector containing the header of serialized metadata.
  // Callers should append the body to the Vector to get the full serialized
  // metadata.
  // The actual body size can be different from `estimated_body_size`.
  static Vector<uint8_t> GetSerializedDataHeader(uint32_t data_type_id,
                                                 wtf_size_t estimated_body_size,
                                                 uint64_t tag = 0) {
    Vector<uint8_t> vector;
    vector.ReserveInitialCapacity(sizeof(CachedMetadataHeader) +
                                  estimated_body_size);
    uint32_t marker = CachedMetadataHandler::kSingleEntryWithTag;
    CHECK_EQ(vector.size(), offsetof(CachedMetadataHeader, marker));
    vector.Append(reinterpret_cast<const uint8_t*>(&marker), sizeof(uint32_t));
    CHECK_EQ(vector.size(), offsetof(CachedMetadataHeader, type));
    vector.Append(reinterpret_cast<const uint8_t*>(&data_type_id),
                  sizeof(uint32_t));
    CHECK_EQ(vector.size(), offsetof(CachedMetadataHeader, tag));
    vector.Append(reinterpret_cast<const uint8_t*>(&tag), sizeof(uint64_t));
    CHECK_EQ(vector.size(), sizeof(CachedMetadataHeader));
    return vector;
  }

  static scoped_refptr<CachedMetadata> Create(uint32_t data_type_id,
                                              const uint8_t* data,
                                              size_t size,
                                              uint64_t tag = 0);
  static scoped_refptr<CachedMetadata> CreateFromSerializedData(
      const uint8_t* data,
      size_t);
  static scoped_refptr<CachedMetadata> CreateFromSerializedData(
      Vector<uint8_t> data);
  static scoped_refptr<CachedMetadata> CreateFromSerializedData(
      mojo_base::BigBuffer& data);

  CachedMetadata(Vector<uint8_t> data, base::PassKey<CachedMetadata>);
  CachedMetadata(uint32_t data_type_id,
                 const uint8_t* data,
                 wtf_size_t size,
                 uint64_t tag,
                 base::PassKey<CachedMetadata>);
  CachedMetadata(mojo_base::BigBuffer data, base::PassKey<CachedMetadata>);
  CachedMetadata(const mojo_base::BigBuffer* data,
                 base::PassKey<CachedMetadata>);

  base::span<const uint8_t> SerializedData() const {
    return base::make_span(RawData(), RawSize());
  }

  uint32_t DataTypeID() const {
    DCHECK_GE(RawSize(), sizeof(CachedMetadataHeader));
    return (reinterpret_cast<const CachedMetadataHeader*>(RawData()))->type;
  }

  const uint8_t* Data() const {
    DCHECK_GE(RawSize(), sizeof(CachedMetadataHeader));
    return RawData() + sizeof(CachedMetadataHeader);
  }

  uint32_t size() const {
    DCHECK_GE(RawSize(), sizeof(CachedMetadataHeader));
    return RawSize() - sizeof(CachedMetadataHeader);
  }

  uint64_t tag() const {
    CHECK_GE(RawSize(), sizeof(CachedMetadataHeader));
    return (reinterpret_cast<const CachedMetadataHeader*>(RawData()))->tag;
  }

  // Drains the serialized data as a Vector<uint8_t> or BigBuffer.
  absl::variant<Vector<uint8_t>, mojo_base::BigBuffer> DrainSerializedData() &&;

 private:
  friend class RefCounted<CachedMetadata>;
  ~CachedMetadata() = default;

  const uint8_t* RawData() const;
  uint32_t RawSize() const;

  // Since the serialization format supports random access, storing it in
  // serialized form avoids need for a copy during serialization.
  absl::variant<Vector<uint8_t>, mojo_base::BigBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CACHED_METADATA_H_
