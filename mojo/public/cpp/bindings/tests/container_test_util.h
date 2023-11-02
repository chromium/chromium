// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINER_TEST_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINER_TEST_UTIL_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"

namespace mojo {

class CopyableType {
 public:
  CopyableType();
  CopyableType(const CopyableType& other);
  CopyableType& operator=(const CopyableType& other);
  ~CopyableType();

  bool copied() const { return copied_; }
  static size_t num_instances() { return num_instances_; }
  CopyableType* ptr() const { return ptr_; }
  void ResetCopied() { copied_ = false; }

 private:
  bool copied_;
  static size_t num_instances_;
  raw_ptr<CopyableType> ptr_;
};

class MoveOnlyType {
 public:
  typedef MoveOnlyType Data_;
  MoveOnlyType();
  MoveOnlyType(MoveOnlyType&& other);
  MoveOnlyType& operator=(MoveOnlyType&& other);

  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;

  ~MoveOnlyType();

  bool moved() const { return moved_; }
  static size_t num_instances() { return num_instances_; }
  MoveOnlyType* ptr() const { return ptr_; }
  void ResetMoved() { moved_ = false; }

 private:
  bool moved_;
  static size_t num_instances_;
  raw_ptr<MoveOnlyType> ptr_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_CONTAINER_TEST_UTIL_H_
