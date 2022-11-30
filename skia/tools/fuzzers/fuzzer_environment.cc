// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_discardable_memory_allocator.h"

namespace {

class Environment {
  base::TestDiscardableMemoryAllocator test_memory_allocator_;

 public:
  Environment() {
    base::DiscardableMemoryAllocator::SetInstance(&test_memory_allocator_);
  }
};

static Environment env;
}  // namespace
