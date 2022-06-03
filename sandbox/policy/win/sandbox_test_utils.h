// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_TEST_UTILS_H_
#define SANDBOX_POLICY_WIN_SANDBOX_TEST_UTILS_H_

#include <string>
#include <vector>

#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {
namespace policy {

constexpr wchar_t kChromeInstallFiles[] = L"chromeInstallFiles";
constexpr wchar_t kLpacChromeInstallFiles[] = L"lpacChromeInstallFiles";

std::vector<Sid> GetCapabilitySids(
    const std::initializer_list<std::wstring>& capabilities);

std::wstring GetAccessAllowedForCapabilities(
    const std::initializer_list<std::wstring>& capabilities);

void EqualSidList(const std::vector<Sid>& left, const std::vector<Sid>& right);

void CheckCapabilities(
    AppContainerBase* profile,
    const std::initializer_list<std::wstring>& additional_capabilities);
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_SANDBOX_TEST_UTILS_H_