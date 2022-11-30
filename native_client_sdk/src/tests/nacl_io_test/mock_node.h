// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_MOCK_NODE_H_
#define TESTS_NACL_IO_TEST_MOCK_NODE_H_

#include "gmock/gmock.h"

#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_handle.h"

class MockNode : public nacl_io::Node {
 public:
  typedef nacl_io::Error Error;
  typedef nacl_io::HandleAttr HandleAttr;
  typedef nacl_io::ScopedNode ScopedNode;

  explicit MockNode(nacl_io::Filesystem*);
  virtual ~MockNode();

  MOCK_METHOD1(Init, Error(int));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD0(FSync, Error());
  MOCK_METHOD1(FTruncate, Error(off_t));
  MOCK_METHOD4(GetDents, Error(size_t, struct dirent*, size_t, int*));
  MOCK_METHOD1(GetStat, Error(struct stat*));
  MOCK_METHOD2(Ioctl, Error(int, va_list));
  MOCK_METHOD4(Read, Error(const HandleAttr&, void*, size_t, int*));
  MOCK_METHOD4(Write, Error(const HandleAttr&, const void*, size_t, int*));
  MOCK_METHOD6(MMap, Error(void*, size_t, int, int, size_t, void**));
  MOCK_METHOD0(GetLinks, int());
  MOCK_METHOD0(GetMode, int());
  MOCK_METHOD0(GetType, int());
  MOCK_METHOD1(GetSize, Error(off_t*));
  MOCK_METHOD0(IsaDir, bool());
  MOCK_METHOD0(IsaFile, bool());
  MOCK_METHOD0(Isatty, Error());
  MOCK_METHOD0(ChildCount, int());
  MOCK_METHOD2(AddChild, Error(const std::string&, const ScopedNode&));
  MOCK_METHOD1(RemoveChild, Error(const std::string&));
  MOCK_METHOD2(FindChild, Error(const std::string&, ScopedNode*));
  MOCK_METHOD0(Link, void());
  MOCK_METHOD0(Unlink, void());
};

#endif  // TESTS_NACL_IO_TEST_MOCK_NODE_H_
