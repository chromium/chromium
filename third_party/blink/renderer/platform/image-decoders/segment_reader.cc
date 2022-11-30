// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

#include <utility>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "third_party/blink/renderer/platform/graphics/rw_buffer.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

namespace {

// Helpers for ROBufferSegmentReader and ParkableImageSegmentReader
template <class Iter>
size_t BufferGetSomeData(Iter& iter,
                         size_t& position_of_block,
                         const char*& data,
                         size_t position) {
  for (size_t size_of_block = iter.size(); size_of_block != 0;
       position_of_block += size_of_block, size_of_block = iter.size()) {
    DCHECK_LE(position_of_block, position);

    if (position_of_block + size_of_block > position) {
      // |position| is in this block.
      const size_t position_in_block = position - position_of_block;
      data = static_cast<const char*>(iter.data()) + position_in_block;
      return size_of_block - position_in_block;
    }

    // Move to next block.
    if (!iter.Next())
      break;
  }
  return 0;
}

template <class Iter>
sk_sp<SkData> BufferCopyAsSkData(Iter iter, size_t available) {
  sk_sp<SkData> data = SkData::MakeUninitialized(available);
  char* dst = static_cast<char*>(data->writable_data());
  do {
    size_t size = iter.size();
    memcpy(dst, iter.data(), size);
    dst += size;
  } while (iter.Next());
  return data;
}

}  // namespace

// SharedBufferSegmentReader ---------------------------------------------------

// Interface for ImageDecoder to read a SharedBuffer.
class SharedBufferSegmentReader final : public SegmentReader {
 public:
  explicit SharedBufferSegmentReader(scoped_refptr<SharedBuffer>);
  SharedBufferSegmentReader(const SharedBufferSegmentReader&) = delete;
  SharedBufferSegmentReader& operator=(const SharedBufferSegmentReader&) =
      delete;
  size_t size() const override;
  size_t GetSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> GetAsSkData() const override;

 private:
  scoped_refptr<SharedBuffer> shared_buffer_;
};

SharedBufferSegmentReader::SharedBufferSegmentReader(
    scoped_refptr<SharedBuffer> buffer)
    : shared_buffer_(std::move(buffer)) {}

size_t SharedBufferSegmentReader::size() const {
  return shared_buffer_->size();
}

size_t SharedBufferSegmentReader::GetSomeData(const char*& data,
                                              size_t position) const {
  data = nullptr;
  auto it = shared_buffer_->GetIteratorAt(position);
  if (it == shared_buffer_->cend())
    return 0;
  data = it->data();
  return it->size();
}

sk_sp<SkData> SharedBufferSegmentReader::GetAsSkData() const {
  sk_sp<SkData> data = SkData::MakeUninitialized(shared_buffer_->size());
  char* buffer = static_cast<char*>(data->writable_data());
  size_t offset = 0;
  for (const auto& span : *shared_buffer_) {
    memcpy(buffer + offset, span.data(), span.size());
    offset += span.size();
  }

  return data;
}

// DataSegmentReader -----------------------------------------------------------

// Interface for ImageDecoder to read an SkData.
class DataSegmentReader final : public SegmentReader {
 public:
  explicit DataSegmentReader(sk_sp<SkData>);
  DataSegmentReader(const DataSegmentReader&) = delete;
  DataSegmentReader& operator=(const DataSegmentReader&) = delete;
  size_t size() const override;
  size_t GetSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> GetAsSkData() const override;

 private:
  sk_sp<SkData> data_;
};

DataSegmentReader::DataSegmentReader(sk_sp<SkData> data)
    : data_(std::move(data)) {}

size_t DataSegmentReader::size() const {
  return data_->size();
}

size_t DataSegmentReader::GetSomeData(const char*& data,
                                      size_t position) const {
  if (position >= data_->size())
    return 0;

  data = reinterpret_cast<const char*>(data_->bytes() + position);
  return data_->size() - position;
}

sk_sp<SkData> DataSegmentReader::GetAsSkData() const {
  return data_;
}

// ROBufferSegmentReader -------------------------------------------------------

class ROBufferSegmentReader final : public SegmentReader {
 public:
  explicit ROBufferSegmentReader(scoped_refptr<ROBuffer>);
  ROBufferSegmentReader(const ROBufferSegmentReader&) = delete;
  ROBufferSegmentReader& operator=(const ROBufferSegmentReader&) = delete;

  size_t size() const override;
  size_t GetSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> GetAsSkData() const override;

 private:
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

size_t ROBufferSegmentReader::GetSomeData(const char*& data,
                                          size_t position) const {
  if (!ro_buffer_)
    return 0;

  base::AutoLock lock(read_lock_);

  if (position < position_of_block_) {
    // ROBuffer::Iter only iterates forwards. Start from the beginning.
    iter_.Reset(ro_buffer_.get());
    position_of_block_ = 0;
  }

  size_t size = BufferGetSomeData(iter_, position_of_block_, data, position);

  if (!iter_.data()) {
    // Reset to the beginning, so future calls can succeed.
    iter_.Reset(ro_buffer_.get());
    position_of_block_ = 0;
  }

  return size;
}

static void UnrefROBuffer(const void* ptr, void* context) {
  static_cast<ROBuffer*>(context)->Release();
}

sk_sp<SkData> ROBufferSegmentReader::GetAsSkData() const {
  if (!ro_buffer_)
    return nullptr;

  // Check to see if the data is already contiguous.
  ROBuffer::Iter iter(ro_buffer_.get());
  const bool multiple_blocks = iter.Next();
  iter.Reset(ro_buffer_.get());

  if (!multiple_blocks) {
    // Contiguous data. No need to copy.
    ro_buffer_->AddRef();
    return SkData::MakeWithProc(iter.data(), iter.size(), &UnrefROBuffer,
                                ro_buffer_.get());
  }

  return BufferCopyAsSkData(iter, ro_buffer_->size());
}

// ParkableImageSegmentReader

class ParkableImageSegmentReader : public SegmentReader {
 public:
  explicit ParkableImageSegmentReader(scoped_refptr<ParkableImage> image);
  ~ParkableImageSegmentReader() override = default;
  size_t size() const override;
  size_t GetSomeData(const char*& data, size_t position) const override;
  sk_sp<SkData> GetAsSkData() const override;
  void LockData() override;
  void UnlockData() override;

 private:
  scoped_refptr<ParkableImage> parkable_image_;
  size_t available_;
};

ParkableImageSegmentReader::ParkableImageSegmentReader(
    scoped_refptr<ParkableImage> image)
    : parkable_image_(std::move(image)), available_(parkable_image_->size()) {
}

size_t ParkableImageSegmentReader::size() const {
  return available_;
}

size_t ParkableImageSegmentReader::GetSomeData(const char*& data,
                                               size_t position) const {
  if (!parkable_image_)
    return 0;

  base::AutoLock lock(parkable_image_->impl_->lock_);
  DCHECK(parkable_image_->impl_->is_locked());

  RWBuffer::ROIter iter(parkable_image_->impl_->rw_buffer_.get(), available_);
  size_t position_of_block = 0;

  return BufferGetSomeData(iter, position_of_block, data, position);
}

sk_sp<SkData> ParkableImageSegmentReader::GetAsSkData() const {
  if (!parkable_image_)
    return nullptr;

  base::AutoLock lock(parkable_image_->impl_->lock_);
  parkable_image_->impl_->Unpark();

  RWBuffer::ROIter iter(parkable_image_->impl_->rw_buffer_.get(), available_);

  if (!iter.HasNext()) {  // No need to copy because the data is contiguous.
    // We lock here so that we don't get a use-after-free. ParkableImage can
    // not be parked while it is locked, so the buffer is valid for the whole
    // lifetime of the SkData. We add the ref so that the ParkableImage has a
    // longer limetime than the SkData.
    parkable_image_->AddRef();
    parkable_image_->LockData();
    return SkData::MakeWithProc(
        iter.data(), available_,
        [](const void* ptr, void* context) -> void {
          auto* parkable_image = static_cast<ParkableImage*>(context);
          {
            base::AutoLock lock(parkable_image->impl_->lock_);
            parkable_image->UnlockData();
          }
          // Don't hold the mutex while we call |Release|, since |Release| can
          // free the ParkableImage, if this is the last reference to it;
          // Freeing the ParkableImage while the mutex is held causes a UAF when
          // the dtor for base::AutoLock is called.
          parkable_image->Release();
        },
        parkable_image_.get());
  }

  // Data is not contiguous so we need to copy.
  return BufferCopyAsSkData(iter, available_);
}

void ParkableImageSegmentReader::LockData() {
  base::AutoLock lock(parkable_image_->impl_->lock_);
  parkable_image_->impl_->Unpark();

  parkable_image_->LockData();
}

void ParkableImageSegmentReader::UnlockData() {
  base::AutoLock lock(parkable_image_->impl_->lock_);

  parkable_image_->UnlockData();
}

// SegmentReader ---------------------------------------------------------------

scoped_refptr<SegmentReader> SegmentReader::CreateFromSharedBuffer(
    scoped_refptr<SharedBuffer> buffer) {
  return base::AdoptRef(new SharedBufferSegmentReader(std::move(buffer)));
}

scoped_refptr<SegmentReader> SegmentReader::CreateFromSkData(
    sk_sp<SkData> data) {
  return base::AdoptRef(new DataSegmentReader(std::move(data)));
}

scoped_refptr<SegmentReader> SegmentReader::CreateFromROBuffer(
    scoped_refptr<ROBuffer> buffer) {
  return base::AdoptRef(new ROBufferSegmentReader(std::move(buffer)));
}

scoped_refptr<SegmentReader> SegmentReader::CreateFromParkableImage(
    scoped_refptr<ParkableImage> image) {
  return base::AdoptRef(new ParkableImageSegmentReader(std::move(image)));
}

}  // namespace blink
