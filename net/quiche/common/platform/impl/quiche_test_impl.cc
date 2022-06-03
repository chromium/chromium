// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quiche/common/platform/impl/quiche_test_impl.h"

#include <string>

#include "base/files/file_path.h"
#include "net/test/test_data_directory.h"

namespace quiche {
namespace test {

std::string QuicheGetCommonSourcePathImpl() {
  base::FilePath net_path = net::GetTestNetDirectory();
  return net_path.AppendASCII("third_party/quiche/common").MaybeAsASCII();
}

}  // namespace test
}  // namespace quiche
