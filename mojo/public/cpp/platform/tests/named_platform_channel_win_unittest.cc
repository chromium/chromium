// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/named_platform_channel.h"

#include <optional>
#include <string>

#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
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

TEST(NamedPlatformChannelWinTest, PipeOwnerMatchesCreatorDefaultOwner) {
  NamedPlatformChannel::Options options;
  options.server_name = NamedPlatformChannel::GenerateRandomServerName();
  NamedPlatformChannel server(options);
  PlatformChannelEndpoint client =
      NamedPlatformChannel::ConnectToServer(options);
  ASSERT_TRUE(client.is_valid());

  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token);
  const base::win::Sid owner = token->Owner();
  EXPECT_EQ(NamedPlatformChannel::IsPipeOwnerPrivileged(
                client.platform_handle().GetHandle().Get()),
            owner == base::win::Sid(base::win::WellKnownSid::kLocalSystem) ||
                owner == base::win::Sid(
                             base::win::WellKnownSid::kBuiltinAdministrators));
}

}  // namespace
}  // namespace mojo
