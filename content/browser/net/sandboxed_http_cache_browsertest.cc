// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "sandbox/policy/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace content {
namespace {

class SandboxedHttpCacheBrowserTest : public ContentBrowserTest {
 public:
  SandboxedHttpCacheBrowserTest() {
    std::vector<base::Feature> enabled_features = {
      net::features::kSandboxHttpCache,
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
      // Network Service Sandboxing is unconditionally enabled on these platforms.
      sandbox::policy::features::kNetworkServiceSandbox,
#endif
    };
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kNetworkServiceInProcess});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    if (!sandbox::policy::features::IsWinNetworkServiceSandboxSupported()) {
      // On *some* Windows, sandboxing cannot be enabled. We skip all the tests
      // on such platforms.
      GTEST_SKIP();
    }
#endif

    // These assertions need to precede ContentBrowserTest::SetUp to prevent the
    // test body from running when one of the assertions fails.
    ASSERT_TRUE(IsOutOfProcessNetworkService());
    ASSERT_TRUE(sandbox::policy::features::IsNetworkSandboxEnabled());

    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SandboxedHttpCacheBrowserTest, OpeningFileIsProhibited) {
  base::RunLoop run_loop;
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());

  absl::optional<bool> result;
  network_service_test.set_disconnect_handler(run_loop.QuitClosure());
  const base::FilePath path =
      GetTestDataFilePath().Append(FILE_PATH_LITERAL("blank.jpg"));
  network_service_test->OpenFile(path, base::BindLambdaForTesting([&](bool b) {
                                   result = b;
                                   run_loop.Quit();
                                 }));
  run_loop.Run();

  EXPECT_EQ(result, absl::make_optional(false));
}

// TODO(yhirano): Add more tests.

}  // namespace
}  // namespace content
