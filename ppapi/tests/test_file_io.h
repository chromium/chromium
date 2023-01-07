// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FILE_IO_H_
#define PPAPI_TESTS_TEST_FILE_IO_H_

#include <stdint.h>

#include <string>

#include "ppapi/tests/test_case.h"

namespace pp {
class FileSystem;
}  // namespace pp

class TestFileIO : public TestCase {
 public:
  explicit TestFileIO(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  enum OpenExpectation {
    CREATE_IF_DOESNT_EXIST = 1 << 0,
    DONT_CREATE_IF_DOESNT_EXIST = 1 << 1,
    OPEN_IF_EXISTS = 1 << 2,
    DONT_OPEN_IF_EXISTS = 1 << 3,
    TRUNCATE_IF_EXISTS = 1 << 4,
    DONT_TRUNCATE_IF_EXISTS = 1 << 5,
    // All values above are defined in pairs: <some_expectation> and
    // DONT_<some_expectation>.
    END_OF_OPEN_EXPECATION_PAIRS = DONT_TRUNCATE_IF_EXISTS,

    INVALID_FLAG_COMBINATION = 1 << 6,
  };

  std::string TestOpen();
  std::string TestOpenDirectory();
  std::string TestReadWriteSetLength();
  std::string TestReadToArrayWriteSetLength();
  std::string TestTouchQuery();
  std::string TestAbortCalls();
  std::string TestParallelReads();
  std::string TestParallelWrites();
  std::string TestNotAllowMixedReadWrite();
  std::string TestRequestOSFileHandle();
  std::string TestRequestOSFileHandleWithOpenExclusive();
  std::string TestMmap();

  // Helper method used by TestOpen().
  // |expectations| is a combination of OpenExpectation values. The followings
  // are considered as valid input:
  // 1) INVALID_FLAG_COMBINATION
  // 2) (DONT_)?CREATE_IF_DOESNT_EXIST | (DONT_)?OPEN_IF_EXISTS |
  //    (DONT_)?TRUNCATE_IF_EXISTS
  std::string MatchOpenExpectations(pp::FileSystem* file_system,
                                    int32_t open_flags,
                                    int32_t expectations);
};

#endif  // PPAPI_TESTS_TEST_FILE_IO_H_
