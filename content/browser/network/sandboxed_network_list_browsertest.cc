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

namespace content {
namespace {

class SandboxedNetworkListBrowserTest : public ContentBrowserTest {
 public:
  SandboxedNetworkListBrowserTest() {
    [[maybe_unused]] bool sandbox_enabled = false;
    [[maybe_unused]] bool lpac_enabled = false;
    switch (GetTestPreCount()) {
      case 0:
        break;
      case 1:
        sandbox_enabled = false;
        lpac_enabled = false;
        break;
      case 2:
        sandbox_enabled = true;
        lpac_enabled = false;
        break;
      case 3:
        sandbox_enabled = true;
        lpac_enabled = true;
        break;
      default:
        NOTREACHED();
    }

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    disabled_features.push_back(features::kNetworkServiceInProcess);

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
    if (sandbox_enabled) {
      enabled_features.push_back(
          sandbox::policy::features::kNetworkServiceSandbox);
    } else {
      disabled_features.push_back(
          sandbox::policy::features::kNetworkServiceSandbox);
    }
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
    if (lpac_enabled) {
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
#endif  // BUILDFLAG(IS_ANDROID)

  void WriteInterfacesToFile(const base::FilePath& path) {
    base::Value::List interfaces_list;
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
      base::Value::Dict interface_dict;
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
