// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_interfaces.h"
#include "sandbox/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/security_descriptor.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {
namespace {

class SandboxedNetworkListBrowserTest : public ContentBrowserTest {
 public:
  SandboxedNetworkListBrowserTest() {
    switch (GetTestPreCount()) {
      case 0:
        break;
      case 1:
        is_sandbox_enabled_ = false;
        is_lpac_enabled_ = false;
        break;
      case 2:
        is_sandbox_enabled_ = true;
        is_lpac_enabled_ = false;
        break;
      case 3:
        is_sandbox_enabled_ = true;
        is_lpac_enabled_ = true;
        break;
      default:
        NOTREACHED();
    }

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    disabled_features.push_back(features::kNetworkServiceInProcess);

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
    if (is_sandbox_enabled_) {
      enabled_features.push_back(
          sandbox::policy::features::kNetworkServiceSandbox);
    } else {
      disabled_features.push_back(
          sandbox::policy::features::kNetworkServiceSandbox);
    }
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
    if (is_lpac_enabled_) {
      enabled_features.push_back(
          sandbox::policy::features::kWinSboxNetworkServiceSandboxIsLPAC);
    } else {
      disabled_features.push_back(
          sandbox::policy::features::kWinSboxNetworkServiceSandboxIsLPAC);
    }
#endif  // BUILDFLAG(IS_WIN)

    scoped_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
#if BUILDFLAG(IS_ANDROID)
  void SetUp() override {
    GTEST_SKIP() << "GetNetworkList not yet supported in Android network "
                    "sandbox. See https://crbug.com/1381381.";
  }
#elif BUILDFLAG(IS_WIN)
  void SetUp() override {
    if (is_sandbox_enabled_) {
      // On Windows, Sandbox::kNetwork always uses an AppContainer (an LPAC when
      // `is_lpac_enabled_` is true, or a standard AppContainer otherwise).
      // Ensure the current window station and thread desktop grant access to
      // the container SID so user32.dll can initialize when
      // CommonSubprocessInit delay-loads it to post WM_NULL in the network
      // service child process.
      const auto sid =
          is_lpac_enabled_
              ? base::win::WellKnownSid::kAllRestrictedApplicationPackages
              : base::win::WellKnownSid::kAllApplicationPackages;
      original_winsta_sd_ = GrantAppContainerAccess(
          ::GetProcessWindowStation(),
          base::win::SecurityObjectType::kWindowStation, sid);
      original_desktop_sd_ =
          GrantAppContainerAccess(::GetThreadDesktop(::GetCurrentThreadId()),
                                  base::win::SecurityObjectType::kDesktop, sid);
    }
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    if (original_desktop_sd_) {
      if (HDESK desktop = ::GetThreadDesktop(::GetCurrentThreadId())) {
        EXPECT_TRUE(original_desktop_sd_->WriteToHandle(
            desktop, base::win::SecurityObjectType::kDesktop,
            DACL_SECURITY_INFORMATION));
      }
    }
    if (original_winsta_sd_) {
      if (HWINSTA winsta = ::GetProcessWindowStation()) {
        EXPECT_TRUE(original_winsta_sd_->WriteToHandle(
            winsta, base::win::SecurityObjectType::kWindowStation,
            DACL_SECURITY_INFORMATION));
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  void WriteInterfacesToFile(const base::FilePath& path) {
    base::ListValue interfaces_list;
    base::RunLoop run_loop;
    std::vector<net::NetworkInterface> interfaces;
    GetNetworkService()->GetNetworkList(
        net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
        base::BindLambdaForTesting(
            [&](const std::optional<std::vector<net::NetworkInterface>>& ret) {
              interfaces = *ret;
              run_loop.Quit();
            }));
    run_loop.Run();

    for (const auto& interface : interfaces) {
      base::DictValue interface_dict;
      interface_dict.Set("name", interface.name);
      interface_dict.Set("type", interface.type);
      interface_dict.Set("address", interface.address.ToString());
      interfaces_list.Append(std::move(interface_dict));
    }

    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(interfaces_list, &json));
    EXPECT_TRUE(base::WriteFile(path, json));
  }

  bool AreFileContentsIdentical(const base::FilePath& path1,
                                const base::FilePath& path2) {
    std::string contents1;
    EXPECT_TRUE(base::ReadFileToString(path1, &contents1));
    std::string contents2;
    EXPECT_TRUE(base::ReadFileToString(path2, &contents2));
    return (contents1 == contents2);
  }

  base::FilePath GetPersistentPathLocation(std::string_view name) {
    return shell()->web_contents()->GetBrowserContext()->GetPath().AppendASCII(
        name);
  }

 private:
  size_t GetTestPreCount() {
    constexpr std::string_view kPreTestPrefix = "PRE_";
    std::string_view test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    size_t count = 0;
    while (base::StartsWith(test_name, kPreTestPrefix)) {
      ++count;
      test_name = test_name.substr(kPreTestPrefix.size());
    }
    return count;
  }

#if BUILDFLAG(IS_WIN)
  std::optional<base::win::SecurityDescriptor> GrantAppContainerAccess(
      HANDLE handle,
      base::win::SecurityObjectType type,
      base::win::WellKnownSid sid) {
    if (!handle) {
      ADD_FAILURE() << "Missing window station or desktop handle";
      return std::nullopt;
    }
    auto sd = base::win::SecurityDescriptor::FromHandle(
        handle, type, DACL_SECURITY_INFORMATION);
    if (!sd) {
      ADD_FAILURE() << "Failed to read DACL from handle";
      return std::nullopt;
    }
    base::win::SecurityDescriptor original_sd = sd->Clone();
    EXPECT_TRUE(sd->SetDaclEntry(sid, base::win::SecurityAccessMode::kGrant,
                                 GENERIC_ALL, 0));
    EXPECT_TRUE(sd->WriteToHandle(handle, type, DACL_SECURITY_INFORMATION));
    return original_sd;
  }

  std::optional<base::win::SecurityDescriptor> original_winsta_sd_;
  std::optional<base::win::SecurityDescriptor> original_desktop_sd_;
#endif  // BUILDFLAG(IS_WIN)

  [[maybe_unused]] bool is_sandbox_enabled_ = false;
  [[maybe_unused]] bool is_lpac_enabled_ = false;
  base::test::ScopedFeatureList scoped_features_;
};

// First part of the test has sandbox enabled, and on Windows, LPAC sandbox also
// enabled.
IN_PROC_BROWSER_TEST_F(SandboxedNetworkListBrowserTest,
                       PRE_PRE_PRE_NetworkList) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteInterfacesToFile(GetPersistentPathLocation("3"));
}

// Second part of the test has sandbox enabled, and on Windows, LPAC sandbox
// also disabled. On non-Windows this makes the test the same as the first
// one above.
IN_PROC_BROWSER_TEST_F(SandboxedNetworkListBrowserTest, PRE_PRE_NetworkList) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteInterfacesToFile(GetPersistentPathLocation("2"));
}

// Third part of the test runs with sandbox disabled.
IN_PROC_BROWSER_TEST_F(SandboxedNetworkListBrowserTest, PRE_NetworkList) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteInterfacesToFile(GetPersistentPathLocation("1"));
}

// The fourth part of the test verifies all the other results match and deletes
// the files.
IN_PROC_BROWSER_TEST_F(SandboxedNetworkListBrowserTest, NetworkList) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(AreFileContentsIdentical(GetPersistentPathLocation("1"),
                                       GetPersistentPathLocation("2")));
  EXPECT_TRUE(AreFileContentsIdentical(GetPersistentPathLocation("1"),
                                       GetPersistentPathLocation("3")));
  base::DeleteFile(GetPersistentPathLocation("1"));
  base::DeleteFile(GetPersistentPathLocation("2"));
  base::DeleteFile(GetPersistentPathLocation("3"));
}

}  // namespace
}  // namespace content
