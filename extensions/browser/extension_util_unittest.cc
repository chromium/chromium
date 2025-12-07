// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "build/android_buildflags.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_set.h"
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

}  // namespace extensions
