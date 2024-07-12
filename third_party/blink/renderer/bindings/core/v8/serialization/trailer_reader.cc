// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_reader.h"

#include "base/numerics/byte_conversions.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"

namespace blink {

TrailerReader::TrailerReader(base::span<const uint8_t> span)
    : iterator_(span) {}

TrailerReader::~TrailerReader() = default;

base::expected<bool, TrailerReader::Error> TrailerReader::SkipToTrailer() {
  DCHECK_EQ(iterator_.position(), 0u);

  auto invalid_header = [this]() {
    iterator_.TruncateTo(0);
    return base::unexpected(Error::kInvalidHeader);
  };
  auto no_trailer = [this]() {
    iterator_.TruncateTo(0);
    return false;
  };

  // We expect to see a version tag. If we see one, proceed.
  // If we don't, maybe it's an old serialized value. If we see nothing at all,
  // this message is apparently blank?
  const uint8_t* byte = iterator_.Object<uint8_t>();
  if (!byte)
    return invalid_header();
  if (*byte != kVersionTag)
    return no_trailer();

  // Read the version as a varint. If it overflows or doesn't terminate, that's
  // a problem.
  uint32_t version = 0;
  unsigned version_shift = 0;
  do {
    byte = iterator_.Object<uint8_t>();
    if (!byte || version_shift >= sizeof(version) * 8)
      return invalid_header();
    version |= (*byte & 0x7F) << version_shift;
    version_shift += 7;
  } while (*byte & 0x80);

  // Validate the version number.
  if (version < kMinWireFormatVersion)
    return no_trailer();
  if (version > SerializedScriptValue::kWireFormatVersion)
    return invalid_header();

  // We expect to see a tag indicating the trailer offset.
  byte = iterator_.Object<uint8_t>();
  if (!byte || *byte != kTrailerOffsetTag)
    return invalid_header();

  // Here and below, note that we cannot simply call BufferIterator::Object for
  // uint64_t, since that would require proper alignment to avoid undefined
  // behavior.
  uint64_t trailer_offset = 0;
  if (auto offset_raw = iterator_.Span<uint8_t, sizeof(uint64_t)>();
      offset_raw.has_value()) {
    trailer_offset = base::U64FromBigEndian(*offset_raw);
  } else {
    return invalid_header();
  }

  uint32_t trailer_size = 0;
  if (auto size_raw = iterator_.Span<uint8_t, sizeof(uint32_t)>();
      size_raw.has_value()) {
    trailer_size = base::U32FromBigEndian(*size_raw);
  } else {
    return invalid_header();
  }

  // If there's no trailer, we're done here.
  if (trailer_size == 0 && trailer_offset == 0)
    return no_trailer();

  // Otherwise, validate that its offset and size are sensible.
  if (trailer_offset < iterator_.position() ||
      base::ClampAdd(trailer_offset, trailer_size) > iterator_.total_size()) {
    return invalid_header();
  }

  iterator_.Seek(static_cast<size_t>(trailer_offset));
  iterator_.TruncateTo(trailer_size);
  return true;
}

base::expected<void, TrailerReader::Error> TrailerReader::Read() {
  while (const uint8_t* tag = iterator_.Object<uint8_t>()) {
    if (*tag != kTrailerRequiresInterfacesTag)
      return base::unexpected(Error::kInvalidTrailer);
    if (required_exposed_interfaces_.size())
      return base::unexpected(Error::kInvalidTrailer);

    uint32_t num_exposed = 0;
    if (auto num_exposed_raw = iterator_.CopyObject<uint32_t>())
      num_exposed = base::ByteSwap(*num_exposed_raw);  // Big-endian.
    else
      return base::unexpected(Error::kInvalidTrailer);

    auto exposed_raw = iterator_.Span<uint8_t>(num_exposed);
    if (exposed_raw.size() != num_exposed)
      return base::unexpected(Error::kInvalidTrailer);

    required_exposed_interfaces_.Grow(num_exposed);
    base::ranges::transform(
        exposed_raw, required_exposed_interfaces_.begin(),
        [](uint8_t raw) { return static_cast<SerializationTag>(raw); });
  }
  return {};
}

}  // namespace blink
