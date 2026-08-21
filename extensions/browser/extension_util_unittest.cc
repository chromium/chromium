// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "build/android_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_set.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace extensions {
namespace {
// Returns a barebones test Extension object with the given name.
static scoped_refptr<const Extension> CreateExtension(const std::string& name) {
  base::FilePath path;
  base::PathService::Get(DIR_TEST_DATA, &path);

  return ExtensionBuilder(name).SetPath(path.AppendASCII(name)).Build();
}
}  // namespace

// Tests that extension URLs are properly mapped to local file paths.
TEST(ExtensionUtilTest, MapUrlToLocalFilePath) {
  scoped_refptr<const Extension> app(CreateExtension("platform_app"));
  ExtensionSet extensions;
  extensions.Insert(app);

  // Non-extension URLs don't map to anything.
  base::FilePath non_extension_path;
  GURL non_extension_url("http://not-an-extension.com/");
  EXPECT_FALSE(util::MapUrlToLocalFilePath(&extensions, non_extension_url,
                                           false, &non_extension_path));
  EXPECT_TRUE(non_extension_path.empty());

  // Valid resources return a valid path.
  base::FilePath valid_path;
  GURL valid_url = app->GetResourceURL("manifest.json");
  EXPECT_TRUE(util::MapUrlToLocalFilePath(
      &extensions, valid_url, true /* use_blocking_api */, &valid_path));
  EXPECT_FALSE(valid_path.empty());

  // A file must exist to be mapped to a path using the blocking API.
  base::FilePath does_not_exist_path;
  GURL does_not_exist_url = app->GetResourceURL("does-not-exist.html");
  EXPECT_FALSE(util::MapUrlToLocalFilePath(&extensions, does_not_exist_url,
                                           true /* use_blocking_api */,
                                           &does_not_exist_path));
  EXPECT_TRUE(does_not_exist_path.empty());

  // A file does not need to exist to be mapped to a path with the non-blocking
  // API. This avoids hitting the disk to see if it exists.
  EXPECT_TRUE(util::MapUrlToLocalFilePath(&extensions, does_not_exist_url,
                                          false /* use_blocking_api */,
                                          &does_not_exist_path));
  EXPECT_FALSE(does_not_exist_path.empty());
}

// TODO(https://crbug.com/356905053):Strict site isolation is not enabled on
// Android, so this test is disabled on desktop android.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_ExtensionIdForSiteInstance DISABLED_ExtensionIdForSiteInstance
#else
#define MAYBE_ExtensionIdForSiteInstance ExtensionIdForSiteInstance
#endif
TEST(ExtensionUtilTest, MAYBE_ExtensionIdForSiteInstance) {
  content::BrowserTaskEnvironment test_environment;
  content::TestBrowserContext test_context;

  // Extension.
  const ExtensionId kExtensionId1(32, 'a');
  scoped_refptr<content::SiteInstance> extension_site_instance =
      content::SiteInstance::CreateForURL(
          &test_context, Extension::GetBaseURLFromExtensionId(kExtensionId1));
  EXPECT_EQ(kExtensionId1,
            util::GetExtensionIdForSiteInstance(*extension_site_instance));

  // GuestView.
  const ExtensionId kExtensionId2(32, 'b');
  scoped_refptr<content::SiteInstance> guest_site_instance =
      content::SiteInstance::CreateForGuest(
          &test_context,
          content::StoragePartitionConfig::Create(&test_context, kExtensionId2,
                                                  "fake_storage_partition_id",
                                                  true /* in_memory */));
  EXPECT_EQ(kExtensionId2,
            util::GetExtensionIdForSiteInstance(*guest_site_instance));

  // Http.
  scoped_refptr<content::SiteInstance> https_site_instance =
      content::SiteInstance::CreateForURL(&test_context,
                                          GURL("https://example.com"));
  EXPECT_EQ("", util::GetExtensionIdForSiteInstance(*https_site_instance));
}

// Verifies that `util::GetURLForExtensionPermissionCheck` returns an empty
// `GURL` when passed a null `content::RenderFrameHost*`.
TEST_F(ExtensionsTest, GetURLForExtensionPermissionCheck_Null) {
  content::RenderFrameHost* null_rfh = nullptr;
  EXPECT_TRUE(util::GetURLForExtensionPermissionCheck(null_rfh).is_empty());
}

// Verifies that `util::GetURLForExtensionPermissionCheck` returns an empty
// `GURL` when passed an uncommitted initial frame.
TEST_F(ExtensionsTest, GetURLForExtensionPermissionCheck_Uncommitted) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        /*instance=*/nullptr);
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(util::GetURLForExtensionPermissionCheck(main_rfh).is_empty());
}

// Verifies that `util::GetURLForExtensionPermissionCheck` returns the committed
// `GURL` for a valid main frame navigation, and returns an empty `GURL` when
// the main frame fails to navigate and commits an error document.
TEST_F(ExtensionsTest, GetURLForExtensionPermissionCheck_MainFrame) {
  // Create a test `content::WebContents`.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        /*instance=*/nullptr);
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  // Simulate a successful navigation to a valid webpage in the main frame.
  const GURL kValidUrl("https://example.com/page.html");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                             kValidUrl);

  // Verify that a committed valid navigation in the main frame returns the
  // committed `GURL`.
  EXPECT_EQ(kValidUrl, util::GetURLForExtensionPermissionCheck(main_rfh));

  // Simulate a failed navigation in the main frame resulting in a top-level
  // error document.
  const GURL kFailedUrl("https://example.com/error.html");
  content::RenderFrameHost* error_rfh =
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents.get(), kFailedUrl, net::ERR_FAILED);
  ASSERT_TRUE(error_rfh);

  // Verify that a top-level error document returns an empty `GURL` instead of
  // the failed destination target URL.
  EXPECT_TRUE(util::GetURLForExtensionPermissionCheck(error_rfh).is_empty());
}

// Verifies that `util::GetURLForExtensionPermissionCheck` returns the committed
// `GURL` for a valid child subframe, returns an empty `GURL` for an uncommitted
// child subframe, and returns an empty `GURL` when the child subframe fails to
// navigate and commits a subframe error document.
TEST_F(ExtensionsTest, GetURLForExtensionPermissionCheck_Subframe) {
  // Create a test `content::WebContents` with a committed main frame.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        /*instance=*/nullptr);
  const GURL kParentUrl("https://example.com/parent.html");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents.get(),
                                                             kParentUrl);
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  // Create a child subframe inside the main frame.
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("child_frame");
  ASSERT_TRUE(child_rfh);

  // Verify that an uncommitted child subframe returns an empty `GURL`.
  EXPECT_TRUE(util::GetURLForExtensionPermissionCheck(child_rfh).is_empty());

  // Simulate a successful committed navigation in the child subframe.
  const GURL kValidChildUrl("https://example.com/child.html");
  content::NavigationSimulator::NavigateAndCommitFromDocument(kValidChildUrl,
                                                              child_rfh);

  // Verify that a committed valid navigation in the child subframe returns the
  // child subframe's committed `GURL`.
  EXPECT_EQ(kValidChildUrl, util::GetURLForExtensionPermissionCheck(child_rfh));

  // Simulate a failed navigation in the child subframe resulting in a subframe
  // error document.
  const GURL kFailedChildUrl("https://example.com/child_error.html");
  content::RenderFrameHost* error_child_rfh =
      content::NavigationSimulator::NavigateAndFailFromDocument(
          kFailedChildUrl, net::ERR_FAILED, child_rfh);
  ASSERT_TRUE(error_child_rfh);

  // Verify that a child subframe error document returns an empty `GURL` instead
  // of the failed child navigation target URL.
  EXPECT_TRUE(
      util::GetURLForExtensionPermissionCheck(error_child_rfh).is_empty());
}

}  // namespace extensions
