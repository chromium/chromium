// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_STREAM_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

class SegmentReader;

class PLATFORM_EXPORT SegmentStream : public SkStream {
 public:
  SegmentStream();
  SegmentStream(const SegmentStream&) = delete;
  SegmentStream& operator=(const SegmentStream&) = delete;
  SegmentStream(SegmentStream&&);
  SegmentStream& operator=(SegmentStream&&);

  ~SegmentStream() override;

  void SetReader(scoped_refptr<SegmentReader>);
  // If a buffer has shrunk beyond the point we have read, it has been cleared.
  // This allows clients to be aware of when data suddenly disappears.
  bool IsCleared() const;

  // From SkStream:
  size_t read(void* buffer, size_t) override;
  size_t peek(void* buffer, size_t) const override;
  bool isAtEnd() const override;
  bool rewind() override;
  bool hasPosition() const override;
  size_t getPosition() const override;
  bool seek(size_t position) override;
  bool move(long offset) override;
  bool hasLength() const override;
  size_t getLength() const override;

 private:
  scoped_refptr<SegmentReader> reader_;
  size_t position_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_SEGMENT_STREAM_H_
