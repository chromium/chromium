// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_test_utils.h"

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/win/security_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace policy {

constexpr wchar_t kBaseSecurityDescriptor[] = L"D:(A;;GA;;;WD)";
constexpr wchar_t kRegistryRead[] = L"registryRead";
constexpr wchar_t klpacPnpNotifications[] = L"lpacPnpNotifications";

std::vector<base::win::Sid> GetCapabilitySids(
    const std::initializer_list<std::wstring>& capabilities) {
  std::vector<base::win::Sid> sids;
  for (const auto& capability : capabilities) {
    sids.push_back(base::win::Sid::FromNamedCapability(capability));
  }
  return sids;
}

std::wstring GetAccessAllowedForCapabilities(
    const std::initializer_list<std::wstring>& capabilities) {
  std::wstring sddl = kBaseSecurityDescriptor;
  for (const auto& capability : GetCapabilitySids(capabilities)) {
    absl::optional<std::wstring> sid_string = capability.ToSddlString();
    CHECK(sid_string);
    base::StrAppend(&sddl, {L"(A;;GRGX;;;", *sid_string, L")"});
  }
  return sddl;
}

void EqualSidList(const std::vector<base::win::Sid>& left,
                  const std::vector<base::win::Sid>& right) {
  EXPECT_EQ(left.size(), right.size());
  auto result = base::ranges::mismatch(left, right);
  EXPECT_EQ(result.first, left.cend());
}

void CheckCapabilities(
    AppContainerBase* profile,
    const std::initializer_list<std::wstring>& additional_capabilities) {
  auto additional_caps = GetCapabilitySids(additional_capabilities);
  auto impersonation_caps =
      GetCapabilitySids({kChromeInstallFiles, klpacPnpNotifications,
                         kLpacChromeInstallFiles, kRegistryRead});
  auto base_caps = GetCapabilitySids(
      {klpacPnpNotifications, kLpacChromeInstallFiles, kRegistryRead});

  base::win::AppendSidVector(impersonation_caps, additional_caps);
  base::win::AppendSidVector(base_caps, additional_caps);

  EqualSidList(impersonation_caps, profile->GetImpersonationCapabilities());
  EqualSidList(base_caps, profile->GetCapabilities());
}
}  // namespace policy
}  // namespace sandbox