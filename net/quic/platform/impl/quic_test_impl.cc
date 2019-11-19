// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_test_impl.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

QuicFlagSaverImpl::QuicFlagSaverImpl() {
#define QUIC_FLAG(type, flag, value) saved_##flag##_ = flag;
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG
}

QuicFlagSaverImpl::~QuicFlagSaverImpl() {
#define QUIC_FLAG(type, flag, value) flag = saved_##flag##_;
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG
}

std::string QuicGetTestMemoryCachePathImpl() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net").AppendASCII("data").AppendASCII(
      "quic_http_response_cache_data");
  // The file path is known to be an ascii string.
  return path.MaybeAsASCII();
}

namespace quic {
ParsedQuicVersionVector AllVersionsExcept99() {
  ParsedQuicVersionVector result;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (version.transport_version != QUIC_VERSION_99) {
      result.push_back(version);
    }
  }
  return result;
}
}  // namespace quic
