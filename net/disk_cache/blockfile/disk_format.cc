// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/disk_format.h"

namespace disk_cache {

static_assert(sizeof(IndexHeader) == 368);

IndexHeader::IndexHeader() {
  memset(this, 0, sizeof(*this));
  magic = kIndexMagic;
  version = kCurrentVersion;
}

}  // namespace disk_cache
