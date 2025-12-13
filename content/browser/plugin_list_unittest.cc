// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_list.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

base::FilePath::CharType kFooPath[] = FILE_PATH_LITERAL("/plugins/foo.plugin");
base::FilePath::CharType kBarPath[] = FILE_PATH_LITERAL("/plugins/bar.plugin");
const char kFooMimeType[] = "application/x-foo-mime-type";
const char kFooFileType[] = "foo";

bool Equals(const WebPluginInfo& a, const WebPluginInfo& b) {
  return (a.name == b.name && a.path == b.path && a.version == b.version &&
          a.desc == b.desc);
}

bool Contains(const std::vector<WebPluginInfo>& list,
              const WebPluginInfo& plugin) {
  for (std::vector<WebPluginInfo>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (Equals(*it, plugin)) {
      return true;
    }
  }
  return false;
}

}  // namespace

class PluginListTest : public testing::Test {
 public:
  PluginListTest()
      : plugin_list_(nullptr, PluginListDeleter),
        foo_plugin_(u"Foo PluginListTest",
                    base::FilePath(kFooPath),
                    u"1.2.3",
                    u"foo"),
        bar_plugin_(u"Bar Plugin", base::FilePath(kBarPath), u"2.3.4", u"bar") {
  }

  void SetUp() override {
    // Needed because `PluginList` is normally a singleton and has a private
    // ctor. Also, `plugin_list_` uses a custom deleter.
    plugin_list_.reset(new PluginList());
    foo_plugin_.mime_types.emplace_back(kFooMimeType, kFooFileType,
                                        std::string());
    plugin_list_->RegisterInternalPlugin(foo_plugin_);
    plugin_list_->RegisterInternalPlugin(bar_plugin_);
  }

  // Needed because `PluginList` is normally a singleton and has a private dtor.
  static void PluginListDeleter(PluginList* plugin_list) { delete plugin_list; }

 protected:
  // Must be first.
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<PluginList, decltype(&PluginListDeleter)> plugin_list_;
  WebPluginInfo foo_plugin_;
  WebPluginInfo bar_plugin_;
};

TEST_F(PluginListTest, GetPlugins) {
  const std::vector<WebPluginInfo>& plugins = plugin_list_->GetPlugins();
  EXPECT_EQ(2u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  EXPECT_TRUE(Contains(plugins, bar_plugin_));
}

TEST_F(PluginListTest, BadPluginDescription) {
  WebPluginInfo plugin_3043(
      std::u16string(), base::FilePath(FILE_PATH_LITERAL("/myplugin.3.0.43")),
      std::u16string(), std::u16string());
  plugin_list_->RegisterInternalPlugin(plugin_3043);
  const std::vector<WebPluginInfo>& plugins = plugin_list_->GetPlugins();
  ASSERT_TRUE(Contains(plugins, plugin_3043));
}

TEST_F(PluginListTest, GetPluginInfoArray) {
  const char kTargetUrl[] = "http://example.com/test.foo";
  GURL target_url(kTargetUrl);
  std::vector<WebPluginInfo> plugins;
  std::vector<std::string> actual_mime_types;

  // Without a GetPlugins() call, the PluginList starts out in an empty state.
  plugin_list_->GetPluginInfoArray(target_url, "application/octet-stream",
                                   &plugins, &actual_mime_types);
  EXPECT_EQ(0u, plugins.size());
  EXPECT_EQ(0u, actual_mime_types.size());

  // Even with the correct MIME type, the empty state means there is no result.
  plugins.clear();
  actual_mime_types.clear();
  plugin_list_->GetPluginInfoArray(target_url, kFooMimeType, &plugins,
                                   &actual_mime_types);
  EXPECT_EQ(0u, plugins.size());
  EXPECT_EQ(0u, actual_mime_types.size());

  plugin_list_->GetPlugins();

  // The file type of the URL is supported by `foo_plugin_`. However,
  // GetPluginInfoArray should not match `foo_plugin_` because the MIME type is
  // application/octet-stream.
  plugin_list_->GetPluginInfoArray(target_url, "application/octet-stream",
                                   &plugins, &actual_mime_types);
  EXPECT_EQ(0u, plugins.size());
  EXPECT_EQ(0u, actual_mime_types.size());

  // `foo_plugin_` matches due to the MIME type.
  plugins.clear();
  actual_mime_types.clear();
  plugin_list_->GetPluginInfoArray(target_url, kFooMimeType, &plugins,
                                   &actual_mime_types);
  EXPECT_EQ(1u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  ASSERT_EQ(1u, actual_mime_types.size());
  EXPECT_EQ(kFooMimeType, actual_mime_types.front());

  // `foo_plugin_` matches due to the file type and empty MIME type.
  plugins.clear();
  actual_mime_types.clear();
  plugin_list_->GetPluginInfoArray(target_url, "", &plugins,
                                   &actual_mime_types);
  EXPECT_EQ(1u, plugins.size());
  EXPECT_TRUE(Contains(plugins, foo_plugin_));
  ASSERT_EQ(1u, actual_mime_types.size());
  EXPECT_EQ(kFooMimeType, actual_mime_types.front());
}

}  // namespace content
