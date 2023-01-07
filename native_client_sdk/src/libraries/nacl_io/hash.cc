// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/hash.h"
#include "nacl_io/osdirent.h"

namespace nacl_io {

ino_t HashPathSegment(ino_t hash, const char* str, size_t len) {
  // First add the path seperator
  hash = (hash * static_cast<ino_t>(33)) ^ '/';
  while (len--) {
    hash = (hash * static_cast<ino_t>(33)) ^ *str++;
  }
  return hash;
}

ino_t HashPath(const Path& path) {
  // Prime the DJB2a hash
  ino_t hash = 5381;

  // Apply a running DJB2a to each part of the path
  for (size_t segment = 0; segment < path.Size(); segment++) {
    const std::string& part = path.Part(segment);
    hash = HashPathSegment(hash, part.c_str(), part.length());
  }
  return hash;
}

}  // namespace nacl_io
