// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/apple/pixel_buffer_pool.h"

#include <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace media {

namespace {
constexpr size_t kMaxNumConsecutiveErrors = 30;
}  // namespace

// static
std::unique_ptr<PixelBufferPool> PixelBufferPool::Create(
    OSType format,
    int width,
    int height,
    std::optional<size_t> max_buffers) {
  NSDictionary* pixel_buffer_attributes = @{
    // Rely on default IOSurface properties.
    CFToNSPtrCast(kCVPixelBufferIOSurfacePropertiesKey) : @{},
    CFToNSPtrCast(kCVPixelBufferPixelFormatTypeKey) : @(format),
    CFToNSPtrCast(kCVPixelBufferWidthKey) : @(width),
    CFToNSPtrCast(kCVPixelBufferHeightKey) : @(height),
  };

  // Create the pool.
  // We don't specify any pool attributes. It is unclear from the documentation
  // what pool attributes are available; they might be
  // kCVPixelBufferPoolMinimumBufferCountKey and
  // kCVPixelBufferPoolMaximumBufferAgeKey unless these are more auxiliary
  // attributes for CVPixelBufferPoolCreatePixelBufferWithAuxAttributes().
  base::apple::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool;
  CVReturn pool_creation_error =
      CVPixelBufferPoolCreate(nil, nil, NSToCFPtrCast(pixel_buffer_attributes),
                              buffer_pool.InitializeInto());
  if (pool_creation_error != noErr) {
    DLOG(ERROR) << "Failed to create CVPixelBufferPool with CVReturn error: "
                << pool_creation_error;
    return nullptr;
  }
  return std::make_unique<PixelBufferPool>(std::move(buffer_pool),
                                           std::move(max_buffers));
}

PixelBufferPool::PixelBufferPool(
    base::apple::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool,
    std::optional<size_t> max_buffers)
    : buffer_pool_(std::move(buffer_pool)),
      max_buffers_(std::move(max_buffers)),
      num_consecutive_errors_(0) {
  DCHECK(buffer_pool_);
}

PixelBufferPool::~PixelBufferPool() {
  // Flushing before freeing probably isn't needed, but it can't hurt.
  Flush();
}

base::apple::ScopedCFTypeRef<CVPixelBufferRef> PixelBufferPool::CreateBuffer() {
  DCHECK(buffer_pool_);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> buffer;
  CVReturn buffer_creation_error;
  if (!max_buffers_.has_value()) {
    buffer_creation_error = CVPixelBufferPoolCreatePixelBuffer(
        nil, buffer_pool_.get(), buffer.InitializeInto());
  } else {
    NSDictionary* attributes = @{
      CFToNSPtrCast(kCVPixelBufferPoolAllocationThresholdKey) :
          @(base::checked_cast<int>(max_buffers_.value()))
    };

    // Specify the allocation threshold using auxiliary attributes.
    buffer_creation_error = CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(
        nil, buffer_pool_.get(), NSToCFPtrCast(attributes),
        buffer.InitializeInto());
  }
  if (buffer_creation_error == kCVReturnWouldExceedAllocationThreshold) {
    LOG(ERROR) << "Cannot exceed the pool's maximum buffer count";
    // kCVReturnWouldExceedAllocationThreshold does not count as an error.
    num_consecutive_errors_ = 0;
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  // If a different error occurred, crash on debug builds or log and return null
  // on release builds.
  DCHECK(buffer_creation_error == noErr)
      << "Failed to create a buffer due to CVReturn error code: "
      << buffer_creation_error;
  if (buffer_creation_error != noErr) {
    LOG(ERROR) << "Failed to create a buffer due to CVReturn error code: "
               << buffer_creation_error;
    ++num_consecutive_errors_;
    CHECK_LE(num_consecutive_errors_, kMaxNumConsecutiveErrors)
        << "Exceeded maximum allowed consecutive error count with error code: "
        << buffer_creation_error;
    return base::apple::ScopedCFTypeRef<CVPixelBufferRef>();
  }
  num_consecutive_errors_ = 0;
  return buffer;
}

void PixelBufferPool::Flush() {
  DCHECK(buffer_pool_);
  CVPixelBufferPoolFlush(buffer_pool_.get(),
                         kCVPixelBufferPoolFlushExcessBuffers);
}

}  // namespace media
