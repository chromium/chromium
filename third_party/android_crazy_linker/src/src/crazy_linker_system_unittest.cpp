// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_system.h"

#include <gtest/gtest.h>
#include <stdlib.h>
#include "crazy_linker_system_mock.h"

namespace crazy {

TEST(System, SingleFile) {
  static const char kPath[] = "/tmp/foo/bar";

  static const char kString[] = "Hello World";
  const size_t kStringLen = sizeof(kString) - 1;

  SystemMock sys;
  sys.AddRegularFile(kPath, kString, kStringLen);

  char buff2[kStringLen + 10];
  FileDescriptor fd(kPath);

  EXPECT_EQ(kStringLen, fd.Read(buff2, sizeof(buff2)));
  buff2[kStringLen] = '\0';
  EXPECT_STREQ(kString, buff2);
}

TEST(System, MakeDirectoryPath) {
  static const struct {
    const char* input;
    const char* expected;
  } kData[] = {
      {"", "./"},       {".", "./"},       {"..", "../"},
      {"./", "./"},     {"../", "../"},    {"foo", "foo/"},
      {"foo/", "foo/"}, {"/foo", "/foo/"}, {"foo/bar", "foo/bar/"},
  };
  for (const auto& data : kData) {
    EXPECT_STREQ(data.expected, MakeDirectoryPath(data.input).c_str())
        << "For [" << data.input << "]";
  }
}

TEST(System, MakeAbsolutePathFrom) {
  SystemMock sys;

  static const struct {
    const char* input;
    const char* expected;
  } kData[] = {
      {"/foo", "/foo"},
      {"/foo/bar/", "/foo/bar/"},
      {"foo", "/home/foo"},
      {"foo/bar", "/home/foo/bar"},
      {"./foo", "/home/./foo"},
      {"../foo", "/home/../foo"},
      {"../../foo", "/home/../../foo"},
  };

  sys.SetCurrentDir("/home");

  for (const auto& data : kData) {
    EXPECT_STREQ(data.expected, MakeAbsolutePathFrom(data.input).c_str())
        << "For [" << data.input << "]";
  }

  for (const auto& data : kData) {
    EXPECT_STREQ(data.expected,
                 MakeAbsolutePathFrom(data.input, strlen(data.input)).c_str())
        << "For [" << data.input << "]";
  }

  sys.SetCurrentDir("/home/");

  for (const auto& data : kData) {
    EXPECT_STREQ(data.expected, MakeAbsolutePathFrom(data.input).c_str())
        << "For [" << data.input << "]";
  }

  for (const auto& data : kData) {
    EXPECT_STREQ(data.expected,
                 MakeAbsolutePathFrom(data.input, strlen(data.input)).c_str())
        << "For [" << data.input << "]";
  }
}

TEST(System, PathExists) {
  SystemMock sys;
  sys.AddRegularFile("/tmp/foo", "FOO", 3);

  EXPECT_TRUE(PathExists("/tmp/foo"));
}

TEST(System, PathExistsWithBadPath) {
  SystemMock sys;
  EXPECT_FALSE(PathExists("/tmp/foo"));
}

TEST(System, IsSystemLibraryPath) {
  static const struct TestPath {
    bool expected;
    const char* path;
  } kTestPaths[] = {
#ifdef __ANDROID__
      {true, "/system/lib/libfoo.so"},
      {true, "/system/lib64/libbar.so"},
      {true, "/system/lib/egl/libEGL_emulation.so"},
      {true, "/vendor/lib/egl/libEGL_swiftshader.so"},
      {true, "/vendor/lib64/libfirmware.so"},
      {false, "/system/app/Foo/lib/libfoo.so"},
      {false, "/system/app/Foo/Foo.apk!lib/x86/libfoo.so"},
#else
      {true, "/usr/lib/libfoo.so"}, {false, "/opt/foo/lib/libfoo.so"},
#endif
  };
  for (const TestPath& path : kTestPaths) {
    EXPECT_EQ(path.expected, IsSystemLibraryPath(path.path)) << path.path;
  }
}

}  // namespace crazy
