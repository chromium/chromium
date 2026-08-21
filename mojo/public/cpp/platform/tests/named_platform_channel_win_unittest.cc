// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

constexpr wchar_t kServerName[] = L"server";

TEST(NamedPlatformChannelWinTest, PipeNameIsUnprefixedByDefault) {
  EXPECT_EQ(L"\\\\.\\pipe\\mojo.server",
            NamedPlatformChannel::GetPipeNameFromServerName(kServerName));
}

TEST(NamedPlatformChannelWinTest, PipeNameUsesAdminProtectedPrefix) {
  EXPECT_EQ(
      L"\\\\.\\pipe\\ProtectedPrefix\\Administrators\\mojo.server",
      NamedPlatformChannel::GetPipeNameFromServerName(
          kServerName, NamedPlatformChannel::PipeNameType::kAdminProtected));
}

TEST(NamedPlatformChannelWinTest, PipeNameUsesLocalSegment) {
  EXPECT_EQ(L"\\\\.\\pipe\\LOCAL\\mojo.server",
            NamedPlatformChannel::GetPipeNameFromServerName(
                kServerName, NamedPlatformChannel::PipeNameType::kLocalPipe));
}

}  // namespace
}  // namespace mojo
