// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"

namespace content {

class WebUIImplTest : public testing::Test {
 protected:
  WebUIDataSourceImpl* CreateDataSource(const std::string& source_name) {
    return new WebUIDataSourceImpl(source_name);
  }
  BrowserTaskEnvironment task_environment;
};

// The URLDataManagerBackend starts with two data source, "chrome://resources"
// and "chrome-untrusted://resources". Currently, only "chrome://resources" is
// supported.
TEST_F(WebUIImplTest, LocalResourceLoaderConfigForDefaultDataSource) {
  URLDataManagerBackend data_backend;
  auto config =
      WebUIImpl::GetLocalResourceLoaderConfigForTesting(&data_backend);
  ASSERT_EQ(config->sources.size(), 1ul);
  const auto& config_source = config->sources[0];
  EXPECT_EQ(config_source->name, "resources");
  EXPECT_TRUE(config_source->headers.starts_with("HTTP/1.1 200 OK"));
  EXPECT_EQ(config_source->should_replace_i18n_in_js, false);
  EXPECT_GT(config_source->path_to_resource_id_map.size(), 0ul);
  EXPECT_GT(config_source->replacement_strings.size(), 0ul);
}

// This test adds a data source with extra string replacements and resource
// paths added to it and ensures this metadata is reflected in the resulting
// Mojo struct.
TEST_F(WebUIImplTest, LocalResourceLoaderConfigForCustomDataSource) {
  URLDataManagerBackend data_backend;
  auto* data_source = CreateDataSource("my-data-source");
  data_source->AddString("a", "b");
  data_source->AddResourcePath("path/to/resource", 42);
  data_source->EnableReplaceI18nInJS();
  data_backend.AddDataSource(data_source);
  auto config =
      WebUIImpl::GetLocalResourceLoaderConfigForTesting(&data_backend);
  ASSERT_EQ(config->sources.size(), 2ul);
  auto it =
      std::find_if(config->sources.begin(), config->sources.end(),
                   [](const blink::mojom::LocalResourceSourcePtr& source) {
                     return source->name == "my-data-source";
                   });
  ASSERT_NE(it, config->sources.end());
  const auto& config_source = *it;
  EXPECT_TRUE(config_source->headers.starts_with("HTTP/1.1 200 OK"));
  EXPECT_EQ(config_source->should_replace_i18n_in_js, true);
  EXPECT_EQ(config_source->path_to_resource_id_map["path/to/resource"], 42);
  EXPECT_EQ(config_source->replacement_strings["a"], "b");
}

}  // namespace content
