// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_READER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

class SkData;
class SkROBuffer;
template <typename T>
class sk_sp;

namespace blink {

// Interface that looks like SharedBuffer. Used by ImageDecoders to use various
// sources of input including:
// - SharedBuffer
//   - for when the caller already has a SharedBuffer
// - SkData
//   - for when the caller already has an SkData
// - SkROBuffer
//   - for when the caller wants to read/write in different threads
//
// Unlike SharedBuffer, this is a read-only interface. There is no way to
// modify the underlying data source.
class PLATFORM_EXPORT SegmentReader
    : public ThreadSafeRefCounted<SegmentReader> {
 public:
  // This version is thread-safe so long as no thread is modifying the
  // underlying SharedBuffer. This class does not modify it, so that would
  // mean modifying it in another way.
  static scoped_refptr<SegmentReader> CreateFromSharedBuffer(
      scoped_refptr<SharedBuffer>);

  // These versions use thread-safe input, so they are always thread-safe.
  static scoped_refptr<SegmentReader> CreateFromSkData(sk_sp<SkData>);
  static scoped_refptr<SegmentReader> CreateFromSkROBuffer(sk_sp<SkROBuffer>);

  SegmentReader() = default;
  virtual ~SegmentReader() = default;
  virtual size_t size() const = 0;
  virtual size_t GetSomeData(const char*& data, size_t position) const = 0;
  virtual sk_sp<SkData> GetAsSkData() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SegmentReader);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_READER_H_
