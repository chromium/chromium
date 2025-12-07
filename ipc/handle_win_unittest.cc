// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_win.h"

#include <windows.h>

#include "base/files/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {

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
