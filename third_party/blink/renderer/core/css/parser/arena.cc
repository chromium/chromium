// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "third_party/blink/renderer/core/css/parser/arena.h"

namespace blink {

void* Arena::SlowAlloc(size_t bytes) {
  if (bytes > next_block_size_) {
    next_block_size_ = bytes;
  }

  std::unique_ptr<char[]> block(new char[next_block_size_]);
  current_ptr_ = block.get();
  end_ptr_ = block.get() + next_block_size_;

  mem_blocks_.push_back(std::move(block));
  next_block_size_ += next_block_size_ / 2;  // Increase by 50%.

  void* ret = current_ptr_;
  current_ptr_ += bytes;
  return ret;
}

}  // namespace blink
