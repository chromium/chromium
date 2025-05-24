// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/files/file.h"
#include "ipc/handle_win.h"
#include "ipc/ipc_platform_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {

TEST(IpcHandleTest, InvalidPlatformFile) {
  // Validate empty and unrepresentable values.
  PlatformFileForTransit pfft = PlatformFileForTransit(::GetCurrentProcess());
  EXPECT_FALSE(pfft.IsValid());
  EXPECT_EQ(pfft.GetHandle(), nullptr);
  pfft = GetPlatformFileForTransit(::GetCurrentThread(), true);
  EXPECT_FALSE(pfft.IsValid());
  EXPECT_EQ(pfft.GetHandle(), nullptr);
  base::File unopened_file;
  pfft = TakePlatformFileForTransit(std::move(unopened_file));
  EXPECT_FALSE(pfft.IsValid());
  EXPECT_EQ(pfft.GetHandle(), nullptr);
}

TEST(IpcHandleTest, InvalidHandleWin) {
  // Validate empty and unrepresentable values.
  HandleWin hw;
  EXPECT_EQ(hw.get_handle(), nullptr);
  hw.set_handle(::GetCurrentProcess());
  EXPECT_EQ(hw.get_handle(), nullptr);
  HandleWin exp = HandleWin(::GetCurrentThread());
  EXPECT_EQ(exp.get_handle(), nullptr);
}

}  // namespace IPC
