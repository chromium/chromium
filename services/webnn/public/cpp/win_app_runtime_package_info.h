// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WIN_APP_RUNTIME_PACKAGE_INFO_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WIN_APP_RUNTIME_PACKAGE_INFO_H_

#include <Windows.h>

#include <appmodel.h>

#include <string_view>

#include "base/strings/cstring_view.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/runtime/WindowsAppSDK-VersionInfo.h"

namespace webnn {

inline constexpr base::wcstring_view kWinAppRuntimePackageFamilyName =
    WINDOWSAPPSDK_RUNTIME_PACKAGE_FRAMEWORK_PACKAGEFAMILYNAME_W;

inline constexpr PACKAGE_VERSION kWinAppRuntimePackageMinVersion = {
    .Major = WINDOWSAPPSDK_RUNTIME_VERSION_MAJOR,
    .Minor = WINDOWSAPPSDK_RUNTIME_VERSION_MINOR,
    .Build = WINDOWSAPPSDK_RUNTIME_VERSION_BUILD,
    .Revision = WINDOWSAPPSDK_RUNTIME_VERSION_REVISION};

inline constexpr std::string_view kWinAppRuntimePackageMinVersionString =
    WINDOWSAPPSDK_RUNTIME_VERSION_DOTQUADSTRING;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WIN_APP_RUNTIME_PACKAGE_INFO_H_
