// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/disk_format.h"

#include <algorithm>
#include <type_traits>

#include "base/containers/span.h"

namespace disk_cache {

static_assert(sizeof(IndexHeader) == 368);

IndexHeader::IndexHeader() {
  std::ranges::fill(base::byte_span_from_ref(*this), 0);
  magic = kIndexMagic;
  version = kCurrentVersion;
}

BlockFileHeader::BlockFileHeader() {
  static_assert(std::has_unique_object_representations_v<BlockFileHeader>);
}

}  // namespace disk_cache
