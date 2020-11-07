// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/font_access/font_access_test_utils.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class FontAccessManagerImplBrowserTest : public ContentBrowserTest {
 public:
  FontAccessManagerImplBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

  void SetUpOnMainThread() override {
    enumeration_cache_ = FontEnumerationCache::GetInstance();
    enumeration_cache_->ResetStateForTesting();
  }

  void TearDownOnMainThread() override {
    font_access_manager()->SkipPrivacyChecksForTesting(false);
  }

  RenderFrameHost* main_rfh() {
    return shell()->web_contents()->GetMainFrame();
  }

  FontAccessManagerImpl* font_access_manager() {
    auto* storage_partition =
        static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());
    return storage_partition->GetFontAccessManager();
  }

  void OverrideFontAccessLocale(const std::string& locale) {
    enumeration_cache_->OverrideLocaleForTesting(locale);
    enumeration_cache_->ResetStateForTesting();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  FontEnumerationCache* enumeration_cache_;
};

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserTest, EnumerationTest) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  font_access_manager()->SkipPrivacyChecksForTesting(true);

  int result = EvalJs(shell(),
                      "(async () => {"
                      "  let count = 0;"
                      "  for await (const item of navigator.fonts.query()) {"
                      "    count++;"
                      "  }"
                      "  return count;"
                      "})()")
                   .ExtractInt();
  ASSERT_GT(result, 0) << "Expected at least one font. Got: " << result;
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(FontAccessManagerImplBrowserTest, LocaleTest) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html")));
  font_access_manager()->SkipPrivacyChecksForTesting(true);

  OverrideFontAccessLocale("zh-cn");

  std::string result =
      EvalJs(shell(),
             "(async () => {"
             "  let fullName = '';"
             "  for await (const item of navigator.fonts.query()) {"
             "    if (item.postscriptName == 'MicrosoftYaHei') {"
             "      fullName = item.fullName;"
             "      break;"
             "    }"
             "  }"
             "  return fullName;"
             "})()")
          .ExtractString();
  std::string ms_yahei_utf8 = "微软雅黑";
  ASSERT_EQ(result, ms_yahei_utf8)
      << "Expected:" << ms_yahei_utf8 << " Got:" << result;
}
#endif

#endif

}  // namespace content
