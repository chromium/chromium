// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vp9_super_frame_bitstream_filter.h"

#include "base/apple/osstatus_logging.h"
#include "base/bits.h"
#include "base/check.h"
#include "base/logging.h"
#include "media/filters/vp9_raw_bits_reader.h"

namespace {

void ReleaseDecoderBuffer(void* refcon,
                          void* doomed_memory_block,
                          size_t size_in_bytes) {
  if (refcon)
    static_cast<media::DecoderBuffer*>(refcon)->Release();
}

// See Annex B of the VP9 specification for details.
// https://www.webmproject.org/vp9/
constexpr uint8_t kSuperFrameMarker = 0b11000000;

}  // namespace

namespace media {

VP9SuperFrameBitstreamFilter::VP9SuperFrameBitstreamFilter() = default;
VP9SuperFrameBitstreamFilter::~VP9SuperFrameBitstreamFilter() = default;

bool VP9SuperFrameBitstreamFilter::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(!buffer->end_of_stream());

  Vp9RawBitsReader reader;
  reader.Initialize(buffer->data(), buffer->data_size());
  const bool show_frame = ShouldShowFrame(&reader);
  if (!reader.IsValid()) {
    DLOG(ERROR) << "Bitstream reading failed.";
    return false;
  }

  // See Vp9Parser::ParseSuperframe() for more details.
  const bool is_superframe =
      (buffer->data()[buffer->data_size() - 1] & 0xE0) == kSuperFrameMarker;
  if (is_superframe && data_) {
    DLOG(WARNING) << "Mixing of superframe and raw frames not supported";
    return false;
  }

  // Passthrough.
  if ((show_frame || is_superframe) && partial_buffers_.empty()) {
    DCHECK(!data_);
    return PreparePassthroughBuffer(std::move(buffer));
  }

  partial_buffers_.emplace_back(std::move(buffer));
  if (!show_frame)
    return true;

  // Time to merge buffers into one superframe.
  return BuildSuperFrame();
}

base::apple::ScopedCFTypeRef<CMBlockBufferRef>
VP9SuperFrameBitstreamFilter::CreatePassthroughBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;

  // The created CMBlockBuffer owns a ref on DecoderBuffer to avoid a copy.
  CMBlockBufferCustomBlockSource source = {0};
  source.refCon = buffer.get();
  source.FreeBlock = &ReleaseDecoderBuffer;

  // Create a memory-backed CMBlockBuffer for the translated data.
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      static_cast<void*>(const_cast<uint8_t*>(buffer->data())),
      buffer->data_size(), kCFAllocatorDefault, &source, 0, buffer->data_size(),
      0, data.InitializeInto());
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status)
        << "CMBlockBufferCreateWithMemoryBlock failed.";
    data.reset();
    return data;
  }
  buffer->AddRef();
  return data;
}

void VP9SuperFrameBitstreamFilter::Flush() {
  partial_buffers_.clear();
  data_.reset();
}

bool VP9SuperFrameBitstreamFilter::ShouldShowFrame(Vp9RawBitsReader* reader) {
  // See section 6.2 of the VP9 specification.
  reader->ReadLiteral(2);  // frame_marker

  uint8_t profile = 0;
  if (reader->ReadBool())  // profile_low_bit
    profile |= 1;
  if (reader->ReadBool())  // profile_high_bit
    profile |= 2;
  if (profile > 2 && reader->ReadBool())  // reserved_zero
    profile += 1;

  if (reader->ReadBool())  // show_existing_frame
    return true;

  reader->ReadBool();         // frame_type
  return reader->ReadBool();  // show_frame
}

bool VP9SuperFrameBitstreamFilter::PreparePassthroughBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  data_ = CreatePassthroughBuffer(std::move(buffer));
  return !!data_;
}

bool VP9SuperFrameBitstreamFilter::AllocateCombinedBlock(size_t total_size) {
  DCHECK(!data_);

  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault, nullptr, total_size, kCFAllocatorDefault, nullptr, 0,
      total_size, 0, data_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status)
        << "CMBlockBufferCreateWithMemoryBlock failed.";
    return false;
  }

  status = CMBlockBufferAssureBlockMemory(data_.get());
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "CMBlockBufferAssureBlockMemory failed.";
    return false;
  }

  return true;
}

bool VP9SuperFrameBitstreamFilter::MergeBuffer(const DecoderBuffer& buffer,
                                               size_t offset) {
  OSStatus status = CMBlockBufferReplaceDataBytes(buffer.data(), data_.get(),
                                                  offset, buffer.data_size());
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "CMBlockBufferReplaceDataBytes failed.";
    return false;
  }

  return true;
}

bool VP9SuperFrameBitstreamFilter::BuildSuperFrame() {
  DCHECK(!partial_buffers_.empty());

  // See Annex B of the VP9 specification for details on this process.

  // Calculate maximum and total size.
  size_t total_size = 0, max_size = 0;
  for (const auto& b : partial_buffers_) {
    total_size += b->data_size();
    if (b->data_size() > max_size)
      max_size = b->data_size();
  }

  const uint8_t bytes_per_frame_size =
      base::bits::AlignUpDeprecatedDoNotUse(
          base::bits::Log2Ceiling(base::checked_cast<uint32_t>(max_size)), 8) /
      8;
  DCHECK_GT(bytes_per_frame_size, 0);
  DCHECK_LE(bytes_per_frame_size, 4u);

  // A leading and trailing marker byte plus storage for each frame size.
  total_size += 2 + bytes_per_frame_size * partial_buffers_.size();

  // Allocate a block to hold the superframe.
  if (!AllocateCombinedBlock(total_size))
    return false;

  // Merge each buffer into our superframe.
  size_t offset = 0;
  for (const auto& b : partial_buffers_) {
    if (!MergeBuffer(*b, offset))
      return false;
    offset += b->data_size();
  }

  // Write superframe trailer which has size information for each buffer.
  size_t trailer_offset = 0;
  const size_t trailer_size = total_size - offset;
  std::unique_ptr<uint8_t[]> trailer(new uint8_t[trailer_size]);

  const uint8_t marker = kSuperFrameMarker + ((bytes_per_frame_size - 1) << 3) +
                         (partial_buffers_.size() - 1);

  trailer[trailer_offset++] = marker;
  for (const auto& b : partial_buffers_) {
    const uint32_t s = base::checked_cast<uint32_t>(b->data_size());
    DCHECK_LE(s, (1ULL << (bytes_per_frame_size * 8)) - 1);

    memcpy(&trailer[trailer_offset], &s, bytes_per_frame_size);
    trailer_offset += bytes_per_frame_size;
  }
  DCHECK_EQ(trailer_offset, trailer_size - 1);
  trailer[trailer_offset] = marker;

  OSStatus status = CMBlockBufferReplaceDataBytes(trailer.get(), data_.get(),
                                                  offset, trailer_size);
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "CMBlockBufferReplaceDataBytes failed.";
    return false;
  }

  partial_buffers_.clear();
  return true;
}

}  // namespace media
