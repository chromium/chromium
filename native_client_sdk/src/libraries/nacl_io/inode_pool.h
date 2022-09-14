// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_INODE_POOL_H_
#define LIBRARIES_NACL_IO_INODE_POOL_H_

#include <stdlib.h>
#include <vector>

#include "nacl_io/osstat.h"
#include "pthread.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

class INodePool {
 public:
  INodePool() : num_nodes_(0), max_nodes_(0) {}

  ino_t Acquire() {
    AUTO_LOCK(lock_);
    const int INO_CNT = 8;

    // If we run out of INO numbers, then allocate 8 more
    if (inos_.size() == 0) {
      max_nodes_ += INO_CNT;
      // Add eight more to the stack in reverse order, offset by 1
      // since '0' refers to no INO.
      for (int a = 0; a < INO_CNT; a++) {
        inos_.push_back(max_nodes_ - a);
      }
    }

    // Return the INO at the top of the stack.
    int val = inos_.back();
    inos_.pop_back();
    num_nodes_++;
    return val;
  }

  void Release(ino_t ino) {
    AUTO_LOCK(lock_);
    inos_.push_back(ino);
    num_nodes_--;
  }

  size_t size() const { return num_nodes_; }
  size_t capacity() const { return max_nodes_; }

 private:
  size_t num_nodes_;
  size_t max_nodes_;
  std::vector<ino_t> inos_;
  sdk_util::SimpleLock lock_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_INODE_POOL_H_
