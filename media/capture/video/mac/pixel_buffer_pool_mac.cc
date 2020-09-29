// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/mac/pixel_buffer_pool_mac.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace media {

namespace {

template <typename T>
class CFNumber {
 public:
  CFNumber(CFNumberType type, T value)
      : value_(value), number_(CFNumberCreate(nil, type, &value_)) {}

  T* get() { return &value_; }
  const T* get() const { return &value_; }
  CFNumberRef cf_number_ref() { return number_.get(); }

 private:
  T value_;
  base::ScopedCFTypeRef<CFNumberRef> number_;
};

base::ScopedCFTypeRef<CFDictionaryRef> CreateEmptyCFDictionary() {
  return base::ScopedCFTypeRef<CFDictionaryRef>(
      CFDictionaryCreate(nil, nil, nil, 0, &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));
}

}  // namespace

// static
std::unique_ptr<PixelBufferPool> PixelBufferPool::Create(
    OSType format,
    int width,
    int height,
    base::Optional<size_t> max_buffers) {
  // Pixel buffer attributes: The attributes of buffers created by the pool.
  CFStringRef pixel_buffer_attribute_keys[] = {
      kCVPixelBufferIOSurfacePropertiesKey,  // We want IOSurfaces.
      kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferWidthKey,
      kCVPixelBufferHeightKey};
  constexpr size_t pixel_buffer_attribute_count =
      sizeof(pixel_buffer_attribute_keys) /
      sizeof(pixel_buffer_attribute_keys[0]);
  // Rely on default IOSurface properties.
  base::ScopedCFTypeRef<CFDictionaryRef> io_surface_options =
      CreateEmptyCFDictionary();
  CFNumber<int> pixel_buffer_format(kCFNumberSInt32Type, format);
  CFNumber<int> pixel_buffer_width(kCFNumberSInt32Type, width);
  CFNumber<int> pixel_buffer_height(kCFNumberSInt32Type, height);
  CFTypeRef pixel_buffer_attribute_values[] = {
      io_surface_options.get(), pixel_buffer_format.cf_number_ref(),
      pixel_buffer_width.cf_number_ref(), pixel_buffer_height.cf_number_ref()};
  static_assert(pixel_buffer_attribute_count ==
                    sizeof(pixel_buffer_attribute_values) /
                        sizeof(pixel_buffer_attribute_values[0]),
                "Key count and value count must match");
  base::ScopedCFTypeRef<CFDictionaryRef> pixel_buffer_attributes(
      CFDictionaryCreate(nil, (const void**)pixel_buffer_attribute_keys,
                         (const void**)pixel_buffer_attribute_values,
                         pixel_buffer_attribute_count,
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));

  // Create the pool.
  // We don't specify any pool attributes. It is unclear from the documentation
  // what pool attributes are available; they might be
  // kCVPixelBufferPoolMinimumBufferCountKey and
  // kCVPixelBufferPoolMaximumBufferAgeKey unless these are more auxiliary
  // attributes for CVPixelBufferPoolCreatePixelBufferWithAuxAttributes().
  base::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool;
  CVReturn pool_creation_error = CVPixelBufferPoolCreate(
      nil, nil, pixel_buffer_attributes.get(), buffer_pool.InitializeInto());
  if (pool_creation_error != noErr) {
    DLOG(ERROR) << "Failed to create CVPixelBufferPool with CVReturn error: "
                << pool_creation_error;
    return nullptr;
  }
  return std::make_unique<PixelBufferPool>(std::move(buffer_pool),
                                           std::move(max_buffers));
}

PixelBufferPool::PixelBufferPool(
    base::ScopedCFTypeRef<CVPixelBufferPoolRef> buffer_pool,
    base::Optional<size_t> max_buffers)
    : buffer_pool_(std::move(buffer_pool)),
      max_buffers_(std::move(max_buffers)) {
  DCHECK(buffer_pool_);
}

PixelBufferPool::~PixelBufferPool() {
  // Flushing before freeing probably isn't needed, but it can't hurt.
  Flush();
}

base::ScopedCFTypeRef<CVPixelBufferRef> PixelBufferPool::CreateBuffer() {
  DCHECK(buffer_pool_);
  base::ScopedCFTypeRef<CVPixelBufferRef> buffer;
  CVReturn buffer_creation_error;
  if (!max_buffers_.has_value()) {
    buffer_creation_error = CVPixelBufferPoolCreatePixelBuffer(
        nil, buffer_pool_, buffer.InitializeInto());
  } else {
    // Specify the allocation threshold using auxiliary attributes.
    CFStringRef attribute_keys[] = {kCVPixelBufferPoolAllocationThresholdKey};
    constexpr size_t attribute_count =
        sizeof(attribute_keys) / sizeof(attribute_keys[0]);
    CFNumber<int> poolAllocationThreshold(
        kCFNumberSInt32Type, base::checked_cast<int>(max_buffers_.value()));
    CFTypeRef attribute_values[] = {poolAllocationThreshold.cf_number_ref()};
    static_assert(attribute_count ==
                      sizeof(attribute_values) / sizeof(attribute_values[0]),
                  "Key count and value count must match");
    base::ScopedCFTypeRef<CFDictionaryRef> attributes(CFDictionaryCreate(
        nil, (const void**)attribute_keys, (const void**)attribute_values,
        attribute_count, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    buffer_creation_error = CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(
        nil, buffer_pool_, attributes.get(), buffer.InitializeInto());
  }
  if (buffer_creation_error == kCVReturnWouldExceedAllocationThreshold) {
    // PixelBufferPool cannot create more buffers.
    return base::ScopedCFTypeRef<CVPixelBufferRef>(nil);
  }
  // If |max_buffers_| wasn't reached, this operation must succeed.
  CHECK(buffer_creation_error == noErr)
      << "Failed to create destination CVPixelBuffer with CVReturn error: "
      << buffer_creation_error;
  return buffer;
}

void PixelBufferPool::Flush() {
  DCHECK(buffer_pool_);
  CVPixelBufferPoolFlush(buffer_pool_, kCVPixelBufferPoolFlushExcessBuffers);
}

}  // namespace media
