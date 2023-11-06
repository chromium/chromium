// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_data_directory.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"

namespace net {

namespace {

// Net directory, relative to source root.
const base::FilePath::CharType kNetRelativePath[] = FILE_PATH_LITERAL("net");

// Net data directory, relative to net directory.
const base::FilePath::CharType kNetDataRelativePath[] =
    FILE_PATH_LITERAL("data");

// Test certificates directory, relative to kNetDataRelativePath.
const base::FilePath::CharType kCertificateDataSubPath[] =
    FILE_PATH_LITERAL("ssl/certificates");

}  // namespace

base::FilePath GetTestNetDirectory() {
  base::FilePath src_root;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root);
  }

  return src_root.Append(kNetRelativePath);
}

base::FilePath GetTestNetDataDirectory() {
  return GetTestNetDirectory().Append(kNetDataRelativePath);
}

base::FilePath GetTestCertsDirectory() {
  return GetTestNetDataDirectory().Append(kCertificateDataSubPath);
}

base::FilePath GetTestClientCertsDirectory() {
  return base::FilePath(kNetDataRelativePath).Append(kCertificateDataSubPath);
}

base::FilePath GetWebSocketTestDataDirectory() {
  base::FilePath data_dir(FILE_PATH_LITERAL("net/data/websocket"));
  return data_dir;
}

}  // namespace net
