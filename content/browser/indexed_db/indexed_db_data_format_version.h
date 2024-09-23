// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_FORMAT_VERSION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_FORMAT_VERSION_H_

#include <cstdint>

#include "base/check_op.h"
#include "content/common/content_export.h"

namespace content::indexed_db {

// Contains version data for the wire format used for encoding IndexedDB values.
// A version tuple (a, b) is at least as new as (a', b')
// iff a >= a' and b >= b'.
class IndexedDBDataFormatVersion {
 public:
  constexpr IndexedDBDataFormatVersion() {}
  constexpr IndexedDBDataFormatVersion(uint32_t v8_version,
                                       uint32_t blink_version)
      : v8_version_(v8_version), blink_version_(blink_version) {}

  static IndexedDBDataFormatVersion GetCurrent() { return current_; }
  static IndexedDBDataFormatVersion& GetMutableCurrentForTesting() {
    return current_;
  }

  uint32_t v8_version() const { return v8_version_; }
  uint32_t blink_version() const { return blink_version_; }

  bool operator==(const IndexedDBDataFormatVersion& other) const {
    return v8_version_ == other.v8_version_ &&
           blink_version_ == other.blink_version_;
  }
  bool operator!=(const IndexedDBDataFormatVersion& other) const {
    return !operator==(other);
  }

  bool IsAtLeast(const IndexedDBDataFormatVersion& other) const {
    return v8_version_ >= other.v8_version_ &&
           blink_version_ >= other.blink_version_;
  }

  // Encodes and decodes the tuple from an int64_t.
  // This scheme is chosen so that earlier versions (before we reported both the
  // Blink and V8 versions) decode properly, with a V8 version of 0.
  int64_t Encode() const {
    // Since negative values are considered invalid, this scheme will only work
    // as long as the v8 version would not overflow int32_t.  We check both
    // components, to be consistent.
    DCHECK_GE(static_cast<int32_t>(v8_version_), 0);
    DCHECK_GE(static_cast<int32_t>(blink_version_), 0);
    return (static_cast<int64_t>(v8_version_) << 32) | blink_version_;
  }
  static IndexedDBDataFormatVersion Decode(int64_t encoded) {
    DCHECK_GE(encoded, 0);
    return IndexedDBDataFormatVersion(encoded >> 32, encoded);
  }

 private:
  uint32_t v8_version_ = 0;
  uint32_t blink_version_ = 0;

  CONTENT_EXPORT static IndexedDBDataFormatVersion current_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_FORMAT_VERSION_H_
