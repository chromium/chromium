// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/buffer_content.h"

#include "base/compiler_specific.h"
#include "base/ranges/algorithm.h"
#include "third_party/tflite/buildflags.h"
#include "third_party/tflite/src/tensorflow/lite/util.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif

namespace webnn::tflite {

namespace {

size_t AddPaddingIfNecessary(size_t size) {
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // The XNNPACK delegate may read up to XNN_EXTRA_BYTES beyond the
  // length of the buffer.
  size += XNN_EXTRA_BYTES;
#endif
  return size;
}

}  // namespace

BufferContent::BufferContent(size_t size)
    : buffer_(base::AlignedAlloc(AddPaddingIfNecessary(size),
                                 ::tflite::kDefaultTensorAlignment)),
      size_(size) {
  // `base::AlignedAlloc` does not return initialized memory.
  base::ranges::fill(AsSpan(), 0);
}

BufferContent::~BufferContent() = default;

base::span<uint8_t> BufferContent::AsSpan() const {
  // SAFETY: `size_` was passed to `base::AlignedAlloc()`.
  return UNSAFE_BUFFERS(
      base::span(reinterpret_cast<uint8_t*>(buffer_.get()), size_));
}

}  // namespace webnn::tflite
