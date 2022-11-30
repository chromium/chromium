// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_MOCK_FS_H_
#define TESTS_NACL_IO_TEST_MOCK_FS_H_

#include "gmock/gmock.h"

#include "nacl_io/filesystem.h"

class MockFs : public nacl_io::Filesystem {
 public:
  typedef nacl_io::Error Error;
  typedef nacl_io::FsInitArgs FsInitArgs;
  typedef nacl_io::Path Path;
  typedef nacl_io::PepperInterface PepperInterface;
  typedef nacl_io::ScopedNode ScopedNode;
  typedef nacl_io::StringMap_t StringMap_t;

  MockFs();
  virtual ~MockFs();

  MOCK_METHOD1(Init, Error(const FsInitArgs&));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD2(Access, Error(const Path&, int));
  MOCK_METHOD3(Open, Error(const Path&, int, ScopedNode*));
  MOCK_METHOD4(OpenWithMode, Error(const Path&, int, mode_t, ScopedNode*));
  MOCK_METHOD2(OpenResource, Error(const Path&, ScopedNode*));
  MOCK_METHOD1(Unlink, Error(const Path&));
  MOCK_METHOD2(Mkdir, Error(const Path&, int));
  MOCK_METHOD1(Rmdir, Error(const Path&));
  MOCK_METHOD1(Remove, Error(const Path&));
  MOCK_METHOD2(Rename, Error(const Path&, const Path&));
};

#endif  // TESTS_NACL_IO_TEST_MOCK_FS_H_
