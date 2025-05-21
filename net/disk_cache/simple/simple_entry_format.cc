// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_entry_format.h"

#include "base/containers/span.h"

namespace disk_cache {

SimpleFileHeader::SimpleFileHeader() {
  // We don't want unset holes in types stored to disk.
  static_assert(std::has_unique_object_representations_v<SimpleFileHeader>,
                "SimpleFileHeader should have no implicit padding bytes");
}

SimpleFileEOF::SimpleFileEOF() {
  // We don't want unset holes in types stored to disk.
  static_assert(std::has_unique_object_representations_v<SimpleFileEOF>,
                "SimpleFileEOF should have no implicit padding bytes");
}

SimpleFileSparseRangeHeader::SimpleFileSparseRangeHeader() {
  // We don't want unset holes in types stored to disk.
  static_assert(
      std::has_unique_object_representations_v<SimpleFileSparseRangeHeader>,
      "SimpleFileSparseRangeHeader should have no implicit padding bytes");
}

}  // namespace disk_cache
