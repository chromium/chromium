// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_test_impl.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

std::string QuicGetTestMemoryCachePathImpl() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net").AppendASCII("data").AppendASCII(
      "quic_http_response_cache_data");
  // The file path is known to be an ascii string.
  return path.MaybeAsASCII();
}

