// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_search_path_list.h"

#include <gtest/gtest.h>
#include "crazy_linker_system_mock.h"
#include "crazy_linker_zip_test_data.h"

namespace crazy {

namespace {

// MAGIC CONSTANT WARNING: These offsets have been determined empirically.
// If you update the content of lib_archive_zip, you will have to adjust
// them for these tests to continue to run.
const int32_t kFooFileOffset = 80;
const int32_t kBarFileOffset = 197;

class TestSystem {
 public:
  TestSystem() : sys_() {
    sys_.AddRegularFile("/tmp/foo/bar", "BARBARBAR", 9);
    sys_.AddRegularFile("/tmp/zoo", "ZOO", 3);
    sys_.AddRegularFile("/foo", "Foo", 3);
    sys_.AddEnvVariable("TEST_LIBRARY_PATH", "/tmp:/");
  }

  ~TestSystem() {}

  void AddFile(const char* path, const char* data, size_t len) {
    sys_.AddRegularFile(path, data, len);
  }

  void SetCurrentDir(const char* path) { sys_.SetCurrentDir(path); }

 private:
  SystemMock sys_;
};

// A mock CPU ABI name used during testing.
// NOTE: This is hard-coded into the zip test tables, do not change it!
const char kTestAbi[] = "test-abi";

// Small structure used to store test data.
// |input_path| is an input file path.
// |expected_path| is either nullptr or an expected file path.
// |expected_offset| is the expected offset (or 0).
struct TestData {
  const char* input_path;
  const char* expected_path;
  int32_t expected_offset;
};

// Perform one single check for the TestData instance |data|, using |list|.
void CheckData(const TestData& data, const SearchPathList& list) {
  auto result = list.FindFile(data.input_path);
  if (data.expected_path) {
    EXPECT_STREQ(data.expected_path, result.path.c_str())
        << "For: " << data.input_path;
    EXPECT_EQ(data.expected_offset, result.offset)
        << "For: " << data.input_path;
  } else {
    EXPECT_STREQ("", result.path.c_str()) << "For: " << data.input_path;
  }
}

}  // namespace

TEST(SearchPathList, Empty) {
  TestSystem sys;
  SearchPathList list;
  sys.SetCurrentDir("/tmp");
  static const TestData kData[] = {
      // Paths without a directory separator should not work.
      {"foo", nullptr, 0},
      // Relative paths should work.
      {"foo/bar", "/tmp/foo/bar", 0},
      // Absolute paths should work
      {"/foo", "/foo", 0},
      {"/tmp/zoo", "/tmp/zoo", 0},
      {"/tmp/foo/bar", "/tmp/foo/bar", 0},
      // File that do not exist should error.
      {"/no-such-file", nullptr, 0},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

TEST(SearchPathList, OneItem) {
  TestSystem sys;
  SearchPathList list;
  list.AddPaths("/tmp/foo");

  static const TestData kData[] = {
      {"bar", "/tmp/foo/bar", 0}, {"zoo", nullptr, 0}, {"foo", nullptr, 0},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

TEST(SearchPathList, Reset) {
  TestSystem sys;
  SearchPathList list;
  list.AddPaths("/tmp/foo");

  auto result = list.FindFile("bar");
  EXPECT_STREQ("/tmp/foo/bar", result.path.c_str());

  list.Reset();
  result = list.FindFile("bar");
  EXPECT_STREQ("", result.path.c_str());
}

TEST(SearchPathList, ResetFromEnv) {
  TestSystem sys;
  SearchPathList list;
  list.ResetFromEnv("TEST_LIBRARY_PATH");

  static const TestData kData[] = {
      // Find file name from env search list.
      {"zoo", "/tmp/zoo", 0},
      {"foo", "/foo", 0},
      // Ignore search list if path contains a directory separator.
      {"foo/bar", nullptr, 0},
      // Or an exclamation mark.
      {"foo!bar", nullptr, 0},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

TEST(SearchPathList, ThreeItems) {
  TestSystem sys;
  SearchPathList list;
  list.AddPaths("/tmp/foo:/tmp/");

  static const TestData kData[] = {
      // Relative path ignores search list. Current directory is /.
      {"foo/bar", nullptr, 0},
      {"tmp/zoo", "/tmp/zoo", 0},
      // Base name uses search list, finds file in /tmp/.
      {"zoo", "/tmp/zoo", 0},
      // Base name uses search list, doesn't find file in / which is no listed.
      {"foo", nullptr, 0},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

TEST(SearchPathList, EnvPathsAfterAddedOnes) {
  TestSystem sys;
  sys.AddFile("/opt/foo", "FOO", 3);
  SearchPathList list;
  list.ResetFromEnv("TEST_LIBRARY_PATH");
  list.AddPaths("/opt");

  // This checks that paths added with AddPaths() have priority over
  // paths added with ResetFromEnv(). An invalid implementation would
  // find '/tmp/foo' instead.
  static const TestData data = {"foo", "/opt/foo", 0};
  CheckData(data, list);
}

TEST(SearchPathList, FindDirectlyInsizeZipArchive) {
  TestSystem sys;
  sys.AddFile("/zips/archive.zip",
              reinterpret_cast<const char*>(testing::lib_archive_zip),
              testing::lib_archive_zip_len);
  sys.SetCurrentDir("/zips");
  // Empty search path list.
  SearchPathList list;

  static const TestData kData[] = {
      // Lookup directly in archive. Full path.
      {"/zips/archive.zip!lib/test-abi/libfoo.so", "/zips/archive.zip",
       kFooFileOffset},

      {"/zips/archive.zip!lib/test-abi/crazy.libbar.so", "/zips/archive.zip",
       kBarFileOffset},

      // Lookup directly in archive, from current directory.
      {"archive.zip!lib/test-abi/libfoo.so", "/zips/archive.zip",
       kFooFileOffset},

      // Cannot find libraries if the zip archive is not in the search list.
      {"libfoo.so", nullptr, 0},
      {"libbar.so", nullptr, 0},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

TEST(SearchPathList, FindInsideListedZipArchive) {
  TestSystem sys;
  sys.AddFile("/zips/archive.zip",
              reinterpret_cast<const char*>(testing::lib_archive_zip),
              testing::lib_archive_zip_len);
  SearchPathList list;
  list.AddPaths("/zips/archive.zip!lib/test-abi/");

  // MAGIC CONSTANT WARNING: These offsets have been determined empirically.
  // If you update the content of lib_archive_zip, you will have to adjust
  // them for these tests to continue to run.
  static const int32_t kFooFileOffset = 80;
  static const int32_t kBarFileOffset = 197;
  static const TestData kData[] = {
      // Lookup directly in archive. Full path.
      {"/zips/archive.zip!lib/test-abi/libfoo.so", "/zips/archive.zip",
       kFooFileOffset},

      {"/zips/archive.zip!lib/test-abi/crazy.libbar.so", "/zips/archive.zip",
       kBarFileOffset},

      // Same, but automatically handle crazy. storage prefix!
      {"/zips/archive.zip!lib/test-abi/libbar.so", "/zips/archive.zip",
       kBarFileOffset},

      // Lookup in archive because it is in the search path.
      {"libfoo.so", "/zips/archive.zip", kFooFileOffset},

      // Same, but automatically handle crazy. storage prefix!
      {"libbar.so", "/zips/archive.zip", kBarFileOffset},
  };
  for (const auto& data : kData) {
    CheckData(data, list);
  }
}

}  // namespace crazy
