// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
#define TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_

#include "fake_ppapi/fake_pepper_interface.h"
#include "gmock/gmock.h"
#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"

#define NULL_NODE ((Node*)NULL)

class DevFsForTesting : public nacl_io::DevFs {
 public:
  explicit DevFsForTesting(nacl_io::PepperInterface* ppapi) {
    nacl_io::FsInitArgs args(1);
    args.ppapi = ppapi;
    Init(args);
  }

  bool Exists(const char* filename) {
    nacl_io::ScopedNode node;
    if (Open(nacl_io::Path(filename), O_RDONLY, &node))
      return false;

    struct stat buf;
    return node->GetStat(&buf) == 0;
  }

  int num_nodes() { return (int)inode_pool_.size(); }
};

#endif  // TESTS_NACL_IO_TEST_DEV_FS_FOR_TESTING_H_
