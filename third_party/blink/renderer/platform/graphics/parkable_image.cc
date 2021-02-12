// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"

#include "base/debug/stack_trace.h"
#include "base/memory/ref_counted.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

void ParkableImage::Append(WTF::SharedBuffer* buffer, size_t offset) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!frozen_);
  for (auto it = buffer->GetIteratorAt(offset); it != buffer->cend(); ++it) {
    DCHECK_GE(buffer->size(), rw_buffer_.size() + it->size());
    const size_t remaining = buffer->size() - rw_buffer_.size() - it->size();
    rw_buffer_.Append(it->data(), it->size(), remaining);
  }
  size_ = rw_buffer_.size();
}

scoped_refptr<SharedBuffer> ParkableImage::Data() const {
  scoped_refptr<ROBuffer> ro_buffer(rw_buffer_.MakeROBufferSnapshot());
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  ROBuffer::Iter it(ro_buffer.get());
  do {
    shared_buffer->Append(static_cast<const char*>(it.data()), it.size());
  } while (it.Next());
  return shared_buffer;
}

scoped_refptr<SegmentReader> ParkableImage::GetSegmentReader() const {
  scoped_refptr<ROBuffer> ro_buffer(rw_buffer_.MakeROBufferSnapshot());
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromROBuffer(std::move(ro_buffer));
  return segment_reader;
}

ParkableImage::ParkableImage(size_t initial_capacity)
    : rw_buffer_(initial_capacity) {
  size_ = rw_buffer_.size();
  ParkableImageManager::Instance().Add(this);
}

ParkableImage::~ParkableImage() {
  ParkableImageManager::Instance().Remove(this);
}

// static
std::unique_ptr<ParkableImage> ParkableImage::Create(size_t initial_capacity) {
  return std::make_unique<ParkableImage>(initial_capacity);
}

scoped_refptr<SegmentReader> ParkableImage::MakeROSnapshot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetSegmentReader();
}

void ParkableImage::Freeze() {
  DCHECK(!frozen_);
  frozen_ = true;
}

size_t ParkableImage::size() const {
  return size_;
}

}  // namespace blink
