// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_test_impl.h"

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "net/test/test_data_directory.h"

namespace quiche::test {

std::string QuicheGetCommonSourcePathImpl() {
  base::FilePath net_path = net::GetTestNetDirectory();
  return net_path.AppendASCII("third_party/quiche/src/quiche/common")
      .MaybeAsASCII();
}

}  // namespace quiche::test

std::string QuicheGetTestMemoryCachePathImpl() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  path = path.AppendASCII("net").AppendASCII("data").AppendASCII(
      "quic_http_response_cache_data");
  // The file path is known to be an ascii string.
  return path.MaybeAsASCII();
}
