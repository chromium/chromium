// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

using base::FilePath;

namespace quic {

// Traverses the directory |dirname| and returns all of the files it contains.
std::vector<std::string> ReadFileContentsImpl(const std::string& dirname) {
  std::vector<std::string> files;
  FilePath directory(FilePath::FromUTF8Unsafe(dirname));
  base::FileEnumerator file_list(directory, true, base::FileEnumerator::FILES);
  for (FilePath file_iter = file_list.Next(); !file_iter.empty();
       file_iter = file_list.Next()) {
    files.push_back(file_iter.AsUTF8Unsafe());
  }
  return files;
}

// Reads the contents of |filename| as a string into |contents|.
void ReadFileContentsImpl(QuicStringPiece filename, std::string* contents) {
  base::ReadFileToString(FilePath::FromUTF8Unsafe(filename), contents);
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_
