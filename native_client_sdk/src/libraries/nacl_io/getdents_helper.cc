// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/getdents_helper.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/log.h"
#include "nacl_io/osinttypes.h"

#include "sdk_util/macros.h"

namespace nacl_io {

GetDentsHelper::GetDentsHelper()
    : curdir_ino_(0), parentdir_ino_(0), init_defaults_(false) {
  Initialize();
}

GetDentsHelper::GetDentsHelper(ino_t curdir_ino, ino_t parentdir_ino)
    : curdir_ino_(curdir_ino),
      parentdir_ino_(parentdir_ino),
      init_defaults_(true) {
  Initialize();
}

void GetDentsHelper::Reset() {
  dirents_.clear();
  Initialize();
}

void GetDentsHelper::Initialize() {
  if (init_defaults_) {
    // Add the default entries: "." and ".."
    AddDirent(curdir_ino_, ".", 1);
    AddDirent(parentdir_ino_, "..", 2);
  }
}

void GetDentsHelper::AddDirent(ino_t ino, const char* name, size_t namelen) {
  assert(name != NULL);
  dirents_.push_back(dirent());
  dirent& entry = dirents_.back();
  entry.d_ino = ino;
  entry.d_reclen = sizeof(dirent);

  if (namelen == 0)
    namelen = strlen(name);

  size_t d_name_max = MEMBER_SIZE(dirent, d_name) - 1;  // -1 for \0.
  size_t copylen = std::min(d_name_max, namelen);
  strncpy(&entry.d_name[0], name, copylen);
  entry.d_name[copylen] = 0;
}

Error GetDentsHelper::GetDents(size_t offs,
                               dirent* pdir,
                               size_t size,
                               int* out_bytes) const {
  *out_bytes = 0;

  // If the buffer pointer is invalid, fail
  if (NULL == pdir) {
    LOG_TRACE("dirent pointer is NULL.");
    return EINVAL;
  }

  // If the buffer is too small, fail
  if (size < sizeof(dirent)) {
    LOG_TRACE("dirent buffer size is too small: %" PRIuS " < %" PRIuS "",
        size, sizeof(dirent));
    return EINVAL;
  }

  // Force size to a multiple of dirent
  size -= size % sizeof(dirent);

  size_t max = dirents_.size() * sizeof(dirent);
  if (offs >= max) {
    // OK, trying to read past the end.
    return 0;
  }

  if (offs + size >= max)
    size = max - offs;

  memcpy(pdir, reinterpret_cast<const char*>(dirents_.data()) + offs, size);
  *out_bytes = size;
  return 0;
}

}  // namespace nacl_io
