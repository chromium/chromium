// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/core_ipcz.h"

#include "base/check.h"
#include "mojo/core/ipcz_api.h"
#include "mojo/public/c/system/thunks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core {
namespace {

// Basic smoke tests for the Mojo Core API as implemented over ipcz.
class CoreIpczTest : public testing::Test {
 public:
  const MojoSystemThunks2& mojo() const { return *mojo_; }
  const IpczAPI& ipcz() const { return GetIpczAPI(); }
  IpczHandle node() const { return GetIpczNode(); }

  CoreIpczTest() { CHECK(InitializeIpczNodeForProcess({.is_broker = true})); }
  ~CoreIpczTest() override { DestroyIpczNodeForProcess(); }

 private:
  const MojoSystemThunks2* const mojo_{GetMojoIpczImpl()};
};

TEST_F(CoreIpczTest, Close) {
  // With ipcz-based Mojo Core, Mojo handles are ipcz handles. So Mojo Close()
  // forwards to ipcz Close().

  IpczHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().OpenPortals(node(), IPCZ_NO_FLAGS, nullptr, &a, &b));

  IpczPortalStatus status = {.size = sizeof(status)};
  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_FALSE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(a));

  EXPECT_EQ(IPCZ_RESULT_OK,
            ipcz().QueryPortalStatus(b, IPCZ_NO_FLAGS, nullptr, &status));
  EXPECT_TRUE(status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED);

  EXPECT_EQ(MOJO_RESULT_OK, mojo().Close(b));
}

}  // namespace
}  // namespace mojo::core
