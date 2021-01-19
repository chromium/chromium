// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_

#include <memory>

#include "base/debug/stack_trace.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/platform/graphics/rw_buffer.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

class SegmentReader;
class ParkableImageManager;

// Wraps a RWBuffer containing encoded image data. In the future, this will be
// written/read from disk when no needed, to improve memory usage.
class PLATFORM_EXPORT ParkableImage final {
 public:
  // |initial_capacity| reserves space in the internal buffer, if you know how
  // much data you'll be appending in advance.
  explicit ParkableImage(size_t initial_capacity = 0);

  ~ParkableImage();

  ParkableImage& operator=(const ParkableImage&) = delete;
  ParkableImage(const ParkableImage&) = delete;

  // Factory method to construct a ParkableImage.
  static std::unique_ptr<ParkableImage> Create(size_t initial_capacity = 0);

  // Freezes the ParkableImage. This changes the following:
  // We are no longer allowed to mutate the internal buffer (e.g. via
  // Append)
  void Freeze();

  // Adds data to the internal buffer of ParkableImage. Cannot be called after
  // the ParkableImage has been frozen (see Freeze()). |offset| is the offset
  // from the start of |buffer| that we want to start copying the data from.
  void Append(WTF::SharedBuffer* buffer, size_t offset = 0);

  // Make a Read-Only snapshot of the data within ParkableImage. This may be a
  // view into the internal buffer of ParkableImage, or a copy of the data. It
  // is guaranteed to be safe to read this data from another thread at any time.
  scoped_refptr<SegmentReader> MakeROSnapshot();

  // Returns the data in the internal buffer. It should not be modified after
  // the ParkableImage has been frozen.
  scoped_refptr<SharedBuffer> Data() const;

  // Returns the size of the internal buffer. Can be called even when
  // ParkableImage has been parked.
  size_t size() const;

 private:
  friend class ParkableImageManager;

  scoped_refptr<SegmentReader> GetSegmentReader() const;

  RWBuffer rw_buffer_;

  size_t size_ = 0;
  bool frozen_ = false;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_H_
