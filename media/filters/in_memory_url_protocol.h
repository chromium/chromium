// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
#define MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Simple FFmpegURLProtocol that reads from a buffer.
// NOTE: This object does not copy the buffer so the
//       buffer pointer passed into the constructor
//       needs to remain valid for the entire lifetime of
//       this object.
class MEDIA_EXPORT InMemoryUrlProtocol : public FFmpegURLProtocol {
 public:
  InMemoryUrlProtocol() = delete;

  InMemoryUrlProtocol(const uint8_t* buf, int64_t size, bool streaming);

  InMemoryUrlProtocol(const InMemoryUrlProtocol&) = delete;
  InMemoryUrlProtocol& operator=(const InMemoryUrlProtocol&) = delete;

  virtual ~InMemoryUrlProtocol();

  // FFmpegURLProtocol methods.
  int Read(int size, uint8_t* data) override;
  bool GetPosition(int64_t* position_out) override;
  bool SetPosition(int64_t position) override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;

 private:
  raw_ptr<const uint8_t, AllowPtrArithmetic | DanglingUntriaged> data_;
  int64_t size_;
  int64_t position_;
  bool streaming_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
