// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_directory_handle_impl.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

constexpr const char* kSafePathComponents[] = {
    "a", "a.txt", "a b.txt", "My Computer", ".a", "lnk.zip", "lnk", "a.local",
};

constexpr const char* kUnsafePathComponents[] = {
    "",
    ".",
    "..",
    "...",
    "con",
    "con.zip",
    "NUL",
    "NUL.zip",
    "a.",
    "a\"a",
    "a<a",
    "a>a",
    "a?a",
    "a/",
    "a\\",
    "a ",
    "a . .",
    " Computer",
    "My Computer.{a}",
    "My Computer.{20D04FE0-3AEA-1069-A2D8-08002B30309D}",
    "a\\a",
    "a.lnk",
    "a/a",
    "a\\a",
    "C:\\",
    "C:/",
    "C:",
};

TEST(NativeFileSystemDirectoryHandleImplTest, IsSafePathComponent) {
  for (const char* component : kSafePathComponents) {
    EXPECT_TRUE(
        NativeFileSystemDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
  for (const char* component : kUnsafePathComponents) {
    EXPECT_FALSE(
        NativeFileSystemDirectoryHandleImpl::IsSafePathComponent(component))
        << component;
  }
}

}  // namespace content
