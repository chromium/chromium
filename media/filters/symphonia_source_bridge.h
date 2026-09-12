// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_SOURCE_BRIDGE_H_
#define MEDIA_FILTERS_SYMPHONIA_SOURCE_BRIDGE_H_

#include <cstddef>
#include <cstdint>

#include "base/compiler_specific.h"
// Required for rust::Slice used by SymphoniaSourceBridge::Read to receive
// buffer slices across the Rust CXX FFI boundary.
#include "third_party/rust/cxx/v1/cxx.h"

namespace media {

// Pure virtual interface bridging media data sources (e.g. DataSource)
// to Symphonia's Rust MediaSource trait.
class SymphoniaSourceBridge {
 public:
  virtual ~SymphoniaSourceBridge() = default;

  // Reads up to buf.size() bytes into buf. Returns the number of bytes read
  // (0 on EOF), or -1 on read error.
  virtual int64_t Read(rust::Slice<uint8_t> buf) = 0;

  // Seeks to the absolute byte position `pos`. Returns true on success.
  virtual bool Seek(uint64_t pos) = 0;

  // Returns the current byte position within the source.
  virtual uint64_t GetPosition() const = 0;

  // Returns the total length in bytes, or -1 if unknown / streaming.
  virtual int64_t GetLength() const = 0;

  // Returns whether the source supports seeking.
  virtual bool IsSeekable() const = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_SOURCE_BRIDGE_H_
