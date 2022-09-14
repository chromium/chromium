// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_GETDENTS_HELPER_H_
#define LIBRARIES_NACL_IO_GETDENTS_HELPER_H_

#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/osdirent.h"

namespace nacl_io {

class GetDentsHelper {
 public:
  // Initialize the helper without any defaults.
  GetDentsHelper();
  GetDentsHelper(ino_t curdir_ino, ino_t parentdir_ino);

  void Reset();
  void AddDirent(ino_t ino, const char* name, size_t namelen);
  Error GetDents(size_t offs, dirent* pdir, size_t size, int* out_bytes) const;

 private:
  void Initialize();

  std::vector<dirent> dirents_;
  ino_t curdir_ino_;
  ino_t parentdir_ino_;
  bool init_defaults_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_GETDENTS_HELPER_H_
