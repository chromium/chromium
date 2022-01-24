// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_test_utils.h"

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace policy {

constexpr wchar_t kBaseSecurityDescriptor[] = L"D:(A;;GA;;;WD)";
constexpr wchar_t kRegistryRead[] = L"registryRead";
constexpr wchar_t klpacPnpNotifications[] = L"lpacPnpNotifications";

std::vector<Sid> GetCapabilitySids(
    const std::initializer_list<std::wstring>& capabilities) {
  std::vector<Sid> sids;
  for (const auto& capability : capabilities) {
    sids.emplace_back(Sid::FromNamedCapability(capability.c_str()));
  }
  return sids;
}

std::wstring GetAccessAllowedForCapabilities(
    const std::initializer_list<std::wstring>& capabilities) {
  std::wstring sddl = kBaseSecurityDescriptor;
  for (const auto& capability : GetCapabilitySids(capabilities)) {
    std::wstring sid_string;
    CHECK(capability.ToSddlString(&sid_string));
    base::StrAppend(&sddl, {L"(A;;GRGX;;;", sid_string, L")"});
  }
  return sddl;
}

void EqualSidList(const std::vector<Sid>& left, const std::vector<Sid>& right) {
  EXPECT_EQ(left.size(), right.size());
  auto result = std::mismatch(left.cbegin(), left.cend(), right.cbegin(),
                              [](const auto& left_sid, const auto& right_sid) {
                                return !!::EqualSid(left_sid.GetPSID(),
                                                    right_sid.GetPSID());
                              });
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

  impersonation_caps.insert(impersonation_caps.end(), additional_caps.begin(),
                            additional_caps.end());
  base_caps.insert(base_caps.end(), additional_caps.begin(),
                   additional_caps.end());

  EqualSidList(impersonation_caps, profile->GetImpersonationCapabilities());
  EqualSidList(base_caps, profile->GetCapabilities());
}
}  // namespace policy
}  // namespace sandbox