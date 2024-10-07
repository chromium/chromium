// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct TestCase {
  std::string url;
  std::string source_name;
};

}  // namespace

namespace content {

class URLDataManagerBackendTest : public testing::TestWithParam<TestCase> {
 protected:
  WebUIDataSourceImpl* CreateWebUIDataSource(const std::string& source_name) {
    return new WebUIDataSourceImpl(source_name);
  }
  BrowserTaskEnvironment task_environment;
};

// A data source should have a scheme matching that of the URL it serves.
TEST_P(URLDataManagerBackendTest, DataSourceSchemeMatchesURLScheme) {
  URLDataManagerBackend data_backend;
  data_backend.AddDataSource(CreateWebUIDataSource(GetParam().source_name));

  GURL url(GetParam().url);
  auto* data_source = data_backend.GetDataSourceFromURL(url);
  ASSERT_TRUE(data_source);
  ASSERT_TRUE(data_source->IsWebUIDataSourceImpl());
  auto scheme = static_cast<WebUIDataSourceImpl*>(data_source)->GetScheme();
  ASSERT_EQ(url.scheme(), scheme);
}

INSTANTIATE_TEST_SUITE_P(
    URLDataManagerBackendTest,
    URLDataManagerBackendTest,
    testing::Values(TestCase{"chrome://test-data-source/", "test-data-source"},
                    TestCase{"chrome-untrusted://test-data-source/",
                             "chrome-untrusted://test-data-source/"},
                    TestCase{"test-data-source://any-host/",
                             "test-data-source://"}));

}  // namespace content
