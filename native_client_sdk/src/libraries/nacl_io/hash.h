// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HASH_H_
#define LIBRARIES_NACL_IO_HASH_H_

#include "nacl_io/osdirent.h"
#include "nacl_io/path.h"

namespace nacl_io {

ino_t HashPathSegment(ino_t hash, const char* str, size_t len);
ino_t HashPath(const Path& path);

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HASH_H_
