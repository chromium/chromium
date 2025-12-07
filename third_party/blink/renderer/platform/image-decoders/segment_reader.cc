// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

namespace {

// Helpers for ROBufferSegmentReader and ParkableImageSegmentReader
template <class Iter>
base::span<const uint8_t> BufferGetSomeData(Iter& iter,
                                            size_t& position_of_block,
                                            size_t position) {
  auto current_span = *iter;
  for (size_t size_of_block = current_span.size(); size_of_block != 0;
       position_of_block += size_of_block,
              size_of_block = current_span.size()) {
    DCHECK_LE(position_of_block, position);

    if (position_of_block + size_of_block > position) {
      // |position| is in this block.
      const size_t position_in_block = position - position_of_block;
      return current_span.subspan(position_in_block);
    }

    // Move to next block.
    if (!iter.Next()) {
      break;
    }
    current_span = *iter;
  }
  return {};
}

template <class Iter>
sk_sp<SkData> BufferCopyAsSkData(Iter iter, size_t available) {
  sk_sp<SkData> data = SkData::MakeUninitialized(available);
  auto dst = skia::as_writable_byte_span(*data);
  do {
    auto src = *iter;
    dst.copy_prefix_from(src);
    dst = dst.subspan(src.size());
  } while (iter.Next());
  return data;
}

}  // namespace

// SharedBufferSegmentReader ---------------------------------------------------

// Interface for ImageDecoder to read a SharedBuffer.
class SharedBufferSegmentReader final : public SegmentReader {
 public:
  explicit SharedBufferSegmentReader(scoped_refptr<const SharedBuffer>);
  SharedBufferSegmentReader(const SharedBufferSegmentReader&) = delete;
  SharedBufferSegmentReader& operator=(const SharedBufferSegmentReader&) =
      delete;
  size_t size() const override;
  base::span<const uint8_t> GetSomeData(size_t position) const override;
  sk_sp<const SkData> GetAsSkData() const override;

 private:
  ~SharedBufferSegmentReader() override = default;
  scoped_refptr<const SharedBuffer> shared_buffer_;
};

SharedBufferSegmentReader::SharedBufferSegmentReader(
    scoped_refptr<const SharedBuffer> buffer)
    : shared_buffer_(std::move(buffer)) {}

size_t SharedBufferSegmentReader::size() const {
  return shared_buffer_->size();
}

base::span<const uint8_t> SharedBufferSegmentReader::GetSomeData(
    size_t position) const {
  auto it = shared_buffer_->GetIteratorAt(position);
  if (it == shared_buffer_->cend()) {
    return {};
  }
  return base::as_byte_span(*it);
}

sk_sp<const SkData> SharedBufferSegmentReader::GetAsSkData() const {
  sk_sp<SkData> data = SkData::MakeUninitialized(shared_buffer_->size());
  auto buffer = skia::as_writable_byte_span(*data);
  for (const auto& span : *shared_buffer_) {
    buffer.take_first(span.size()).copy_from(base::as_bytes(span));
  }
  return data;
}

// DataSegmentReader -----------------------------------------------------------

// Interface for ImageDecoder to read an SkData.
class DataSegmentReader final : public SegmentReader {
 public:
  explicit DataSegmentReader(sk_sp<const SkData>);
  DataSegmentReader(const DataSegmentReader&) = delete;
  DataSegmentReader& operator=(const DataSegmentReader&) = delete;
  size_t size() const override;
  base::span<const uint8_t> GetSomeData(size_t position) const override;
  sk_sp<const SkData> GetAsSkData() const override;

 private:
  ~DataSegmentReader() override = default;
  sk_sp<const SkData> data_;
};

DataSegmentReader::DataSegmentReader(sk_sp<const SkData> data)
    : data_(std::move(data)) {}

size_t DataSegmentReader::size() const {
  return data_->size();
}

base::span<const uint8_t> DataSegmentReader::GetSomeData(
    size_t position) const {
  if (position >= data_->size()) {
    return {};
  }
  return skia::as_byte_span(*data_).subspan(position);
}

sk_sp<const SkData> DataSegmentReader::GetAsSkData() const {
  return data_;
}

// ROBufferSegmentReader -------------------------------------------------------

class ROBufferSegmentReader final : public SegmentReader {
 public:
  explicit ROBufferSegmentReader(scoped_refptr<ROBuffer>);
  ROBufferSegmentReader(const ROBufferSegmentReader&) = delete;
  ROBufferSegmentReader& operator=(const ROBufferSegmentReader&) = delete;

  size_t size() const override;
  base::span<const uint8_t> GetSomeData(size_t position) const override;
  sk_sp<const SkData> GetAsSkData() const override;

 private:
  ~ROBufferSegmentReader() override = default;
  scoped_refptr<ROBuffer> ro_buffer_;
  mutable base::Lock read_lock_;
  // Position of the first char in the current block of iter_.
  mutable size_t position_of_block_ GUARDED_BY(read_lock_);
  mutable ROBuffer::Iter iter_ GUARDED_BY(read_lock_);
};

ROBufferSegmentReader::ROBufferSegmentReader(scoped_refptr<ROBuffer> buffer)
    : ro_buffer_(std::move(buffer)),
      position_of_block_(0),
      iter_(ro_buffer_.get()) {}

size_t ROBufferSegmentReader::size() const {
  return ro_buffer_ ? ro_buffer_->size() : 0;
}

base::span<const uint8_t> ROBufferSegmentReader::GetSomeData(
    size_t position) const {
  if (!ro_buffer_) {
    return {};
  }

  base::AutoLock lock(read_lock_);

  if (position < position_of_block_) {
    // ROBuffer::Iter only iterates forwards. Start from the beginning.
    iter_.Reset(ro_buffer_.get());
    position_of_block_ = 0;
  }

  auto data = BufferGetSomeData(iter_, position_of_block_, position);

  if ((*iter_).empty()) {
    // Reset to the beginning, so future calls can succeed.
    iter_.Reset(ro_buffer_.get());
    position_of_block_ = 0;
  }

  return data;
}

static void UnrefROBuffer(const void* ptr, void* context) {
  static_cast<ROBuffer*>(context)->Release();
}

sk_sp<const SkData> ROBufferSegmentReader::GetAsSkData() const {
  if (!ro_buffer_) {
    return nullptr;
  }

  // Check to see if the data is already contiguous.
  ROBuffer::Iter iter(ro_buffer_.get());
  const bool multiple_blocks = iter.Next();
  iter.Reset(ro_buffer_.get());

  if (!multiple_blocks) {
    // Contiguous data. No need to copy.
    ro_buffer_->AddRef();
    auto data = *iter;
    return SkData::MakeWithProc(data.data(), data.size(), &UnrefROBuffer,
                                ro_buffer_.get());
  }

  return BufferCopyAsSkData(iter, ro_buffer_->size());
}

// SegmentReader ---------------------------------------------------------------

scoped_refptr<SegmentReader> SegmentReader::CreateFromSharedBuffer(
    scoped_refptr<const SharedBuffer> buffer) {
  return base::AdoptRef(new SharedBufferSegmentReader(std::move(buffer)));
}

scoped_refptr<SegmentReader> SegmentReader::CreateFromSkData(
    sk_sp<const SkData> data) {
  return base::AdoptRef(new DataSegmentReader(std::move(data)));
}

scoped_refptr<SegmentReader> SegmentReader::CreateFromROBuffer(
    scoped_refptr<ROBuffer> buffer) {
  return base::AdoptRef(new ROBufferSegmentReader(std::move(buffer)));
}

// static
sk_sp<SkData> SegmentReader::RWBufferCopyAsSkData(RWBuffer::ROIter iter,
                                                  size_t available) {
  return BufferCopyAsSkData(iter, available);
}

// static
base::span<const uint8_t> SegmentReader::RWBufferGetSomeData(
    RWBuffer::ROIter& iter,
    size_t& position_of_block,
    size_t position) {
  return BufferGetSomeData(iter, position_of_block, position);
}

}  // namespace blink
