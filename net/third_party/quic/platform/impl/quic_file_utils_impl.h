// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_

#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/impl/quic_file_utils_impl.h"

using base::FilePath;

namespace quic {

// Traverses the directory |dirname| and retuns all of the files
// it contains.
std::vector<QuicString> ReadFileContentsImpl(const QuicString& dirname) {
  std::vector<QuicString> files;
  FilePath directory(FilePath::FromUTF8Unsafe(dirname));
  base::FileEnumerator file_list(directory, true, base::FileEnumerator::FILES);
  for (FilePath file_iter = file_list.Next(); !file_iter.empty();
       file_iter = file_list.Next()) {
    files.push_back(file_iter.AsUTF8Unsafe());
  }
  return files;
}

// Reads the contents of |filename| as a string into |contents|.
void ReadFileContentsImpl(const QuicString& filename, QuicString* contents) {
  base::ReadFileToString(FilePath::FromUTF8Unsafe(filename), contents);
}

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_FILE_UTILS_IMPL_H_
