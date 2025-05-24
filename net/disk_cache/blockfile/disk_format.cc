// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "net/disk_cache/blockfile/disk_format.h"

namespace disk_cache {

static_assert(sizeof(IndexHeader) == 368);

IndexHeader::IndexHeader() {
  memset(this, 0, sizeof(*this));
  magic = kIndexMagic;
  version = kCurrentVersion;
}

}  // namespace disk_cache
