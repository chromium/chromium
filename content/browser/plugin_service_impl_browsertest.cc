// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PluginServiceImplBrowserTest : public ContentBrowserTest {
 public:
  PluginServiceImplBrowserTest()
      : plugin_path_(FILE_PATH_LITERAL("internal-nonesuch")),
        profile_dir_(FILE_PATH_LITERAL("/fake/user/foo/dir")) {}

  ~PluginServiceImplBrowserTest() override = default;

  void RegisterFakePlugin() {
    WebPluginInfo fake_info;
    fake_info.name = u"fake_plugin";
    fake_info.path = plugin_path_;

    PluginServiceImpl* service = PluginServiceImpl::GetInstance();
    service->RegisterInternalPlugin(fake_info);
    service->Init();
    service->GetPlugins();
  }

  base::FilePath plugin_path_;
  base::FilePath profile_dir_;
};

IN_PROC_BROWSER_TEST_F(PluginServiceImplBrowserTest,
                       GetPluginInfoByPathForTesting) {
  RegisterFakePlugin();

  PluginServiceImpl* service = PluginServiceImpl::GetInstance();

  std::optional<WebPluginInfo> plugin_info =
      service->GetPluginInfoByPathForTesting(plugin_path_);
  ASSERT_TRUE(plugin_info.has_value());

  EXPECT_EQ(plugin_path_, plugin_info.value().path);
}

}  // namespace content
