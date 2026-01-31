// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/origin.h"

namespace content {
namespace {

class MockWebUIController : public WebUIController {
 public:
  explicit MockWebUIController(WebUI* web_ui) : WebUIController(web_ui) {}
  ~MockWebUIController() override = default;

  void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config,
      const url::Origin& current_origin) override {
    if (populate_callback_) {
      populate_callback_.Run(config, current_origin);
    }
  }

  void SetPopulateCallback(
      base::RepeatingCallback<void(blink::mojom::LocalResourceLoaderConfig*,
                                   const url::Origin&)> callback) {
    populate_callback_ = std::move(callback);
  }

 private:
  base::RepeatingCallback<void(blink::mojom::LocalResourceLoaderConfig*,
                               const url::Origin&)>
      populate_callback_;
};

}  // namespace

class WebUIImplTestBase {
 protected:
  WebUIDataSourceImpl* CreateDataSource(const std::string& source_name) {
    return new WebUIDataSourceImpl(source_name);
  }
};

class WebUIImplLocalResourceLoaderConfigTest : public WebUIImplTestBase,
                                               public testing::Test {
 protected:
  BrowserTaskEnvironment task_environment;
};

// The URLDataManagerBackend starts with two data source, "chrome://resources"
// and "chrome-untrusted://resources". Currently, only "chrome://resources" is
// supported.
TEST_F(WebUIImplLocalResourceLoaderConfigTest,
       LocalResourceLoaderConfigForDefaultDataSource) {
  URLDataManagerBackend data_backend;

  auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
      &data_backend, url::Origin::Create(GURL("chrome://resources/")),
      /*controller=*/nullptr);

  url::Origin origin = url::Origin::Create(GURL("chrome://resources/"));
  const auto& config_source = config->sources[origin];
  ASSERT_TRUE(config_source);
  EXPECT_TRUE(config_source->headers.starts_with("HTTP/1.1 200 OK"));
  EXPECT_EQ(config_source->should_replace_i18n_in_js, false);
  EXPECT_GT(config_source->path_to_resource_map.size(), 0ul);
  EXPECT_GT(config_source->replacement_strings.size(), 0ul);
  EXPECT_TRUE(
      config_source->path_to_resource_map.begin()->second->is_resource_id());
}

// This test adds a data source with extra string replacements and resource
// paths added to it and ensures this metadata is reflected in the resulting
// Mojo struct.
TEST_F(WebUIImplLocalResourceLoaderConfigTest,
       LocalResourceLoaderConfigForCustomDataSource) {
  URLDataManagerBackend data_backend;
  auto* data_source = CreateDataSource("my-data-source");
  data_source->AddString("a", "b");
  data_source->AddResourcePath("path/to/resource", 42);
  data_source->EnableReplaceI18nInJS();
  data_backend.AddDataSource(data_source);

  auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
      &data_backend, url::Origin::Create(GURL("chrome://my-data-source")),
      /*controller=*/nullptr);

  url::Origin origin = url::Origin::Create(GURL("chrome://my-data-source"));
  const auto& config_source = config->sources[origin];
  ASSERT_TRUE(config_source);
  EXPECT_TRUE(config_source->headers.starts_with("HTTP/1.1 200 OK"));
  EXPECT_EQ(config_source->should_replace_i18n_in_js, true);
  EXPECT_EQ(config_source->path_to_resource_map["path/to/resource"]
                ->get_resource_id(),
            42);
  EXPECT_EQ(config_source->replacement_strings["a"], "b");
}

// This test adds a data source with strings.m.js enabled and ensures the
// resulting Mojo struct contains the generated strings JS in the
// `path_to_response_map`.
TEST_F(WebUIImplLocalResourceLoaderConfigTest,
       LocalResourceLoaderConfigForStringsPath) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUIInProcessResourceLoadingV2);
  URLDataManagerBackend data_backend;
  auto* data_source = CreateDataSource("my-data-source");
  data_source->UseStringsJs();
  data_source->AddString("foo", "bar");
  data_backend.AddDataSource(data_source);

  auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
      &data_backend, url::Origin::Create(GURL("chrome://my-data-source")),
      /*controller=*/nullptr);

  url::Origin origin = url::Origin::Create(GURL("chrome://my-data-source"));
  const auto& config_source = config->sources[origin];
  ASSERT_TRUE(config_source);
  EXPECT_TRUE(config_source->path_to_resource_map.contains("strings.m.js"));
  EXPECT_TRUE(config_source->path_to_resource_map.at("strings.m.js")
                  ->is_response_body());
  EXPECT_NE(config_source->path_to_resource_map.at("strings.m.js")
                ->get_response_body()
                .find("foo"),
            std::string::npos);
  EXPECT_NE(config_source->path_to_resource_map.at("strings.m.js")
                ->get_response_body()
                .find("bar"),
            std::string::npos);
  EXPECT_EQ(config_source->replacement_strings["foo"], "bar");
}

// This test adds a data source with a custom resource path mapped to a response
// and ensures the resulting Mojo struct contains the response in the
// `path_to_response_map`.
TEST_F(WebUIImplLocalResourceLoaderConfigTest,
       LocalResourceLoaderConfigWithPreloadedResources) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUIInProcessResourceLoadingV2);
  URLDataManagerBackend data_backend;
  auto* data_source = CreateDataSource("my-data-source");
  data_source->SetResourcePathToResponse("colors.css", "body { color: red; }");
  data_backend.AddDataSource(data_source);

  auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
      &data_backend, url::Origin::Create(GURL("chrome://my-data-source")),
      /*controller=*/nullptr);

  url::Origin origin = url::Origin::Create(GURL("chrome://my-data-source"));
  const auto& config_source = config->sources[origin];
  ASSERT_TRUE(config_source);
  EXPECT_TRUE(config_source->path_to_resource_map.contains("colors.css"));
  EXPECT_TRUE(
      config_source->path_to_resource_map.at("colors.css")->is_response_body());
  EXPECT_EQ(
      config_source->path_to_resource_map.at("colors.css")->get_response_body(),
      "body { color: red; }");
}

// Verifies that `LocalResourceLoaderConfig` includes resources ONLY for the
// current WebUI and shared sources, excluding other WebUIs.
TEST_F(WebUIImplLocalResourceLoaderConfigTest, ResourcesIsolatedPerWebUI) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUIInProcessResourceLoadingV2);
  URLDataManagerBackend data_backend;

  auto* source1 = CreateDataSource("host1");
  source1->SetResourcePathToResponse("res1", "content1");
  data_backend.AddDataSource(source1);

  auto* source2 = CreateDataSource("host2");
  source2->SetResourcePathToResponse("res2", "content2");
  data_backend.AddDataSource(source2);

  // Test for host1
  {
    auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
        &data_backend, url::Origin::Create(GURL("chrome://host1")),
        /*controller=*/nullptr);

    // Check that config contains source for host1.
    EXPECT_TRUE(
        config->sources.contains(url::Origin::Create(GURL("chrome://host1"))));
    // Check that config DOES NOT contain source for host2.
    EXPECT_FALSE(
        config->sources.contains(url::Origin::Create(GURL("chrome://host2"))));

    // Check that resources are populated for host1.
    const auto& config_source1 =
        config->sources[url::Origin::Create(GURL("chrome://host1"))];
    ASSERT_TRUE(config_source1);
    EXPECT_TRUE(config_source1->path_to_resource_map.contains("res1"));
    EXPECT_EQ(
        config_source1->path_to_resource_map.at("res1")->get_response_body(),
        "content1");
  }

  // Test for host2
  {
    auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
        &data_backend, url::Origin::Create(GURL("chrome://host2")),
        /*controller=*/nullptr);

    // Check that config contains source for host2.
    EXPECT_TRUE(
        config->sources.contains(url::Origin::Create(GURL("chrome://host2"))));
    // Check that config DOES NOT contain source for host1.
    EXPECT_FALSE(
        config->sources.contains(url::Origin::Create(GURL("chrome://host1"))));

    // Check that resources are populated for host2.
    const auto& config_source2 =
        config->sources[url::Origin::Create(GURL("chrome://host2"))];
    ASSERT_TRUE(config_source2);
    EXPECT_TRUE(config_source2->path_to_resource_map.contains("res2"));
    EXPECT_EQ(
        config_source2->path_to_resource_map.at("res2")->get_response_body(),
        "content2");
  }
}

class WebUIImplRenderViewHostTest : public RenderViewHostTestHarness,
                                    public WebUIImplTestBase {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebUIInProcessResourceLoadingV2);
  }

  void AddDataSource(const std::string& source_name) {
    URLDataManagerBackend* data_backend =
        URLDataManagerBackend::GetForBrowserContext(browser_context());
    auto* data_source = CreateDataSource(source_name);
    data_source->SetResourcePathToResponse("test.js", "content");
    data_backend->AddDataSource(data_source);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WebUIImplRenderViewHostTest,
       GetLocalResourceLoaderConfigMatchingOrigin) {
  std::unique_ptr<WebUIImpl> web_ui =
      std::make_unique<WebUIImpl>(web_contents());
  web_ui->SetRenderFrameHost(main_rfh());
  AddDataSource("my-data-source");
  auto origin = url::Origin::Create(GURL("chrome://my-data-source"));

  auto config = web_ui->GetLocalResourceLoaderConfigForTesting(
      URLDataManagerBackend::GetForBrowserContext(browser_context()), origin,
      /*controller=*/nullptr);

  EXPECT_TRUE(config->sources.contains(origin));
}

TEST_F(WebUIImplRenderViewHostTest,
       GetLocalResourceLoaderConfigMismatchingOrigin) {
  std::unique_ptr<WebUIImpl> web_ui =
      std::make_unique<WebUIImpl>(web_contents());
  web_ui->SetRenderFrameHost(main_rfh());
  AddDataSource("my-data-source");
  auto origin = url::Origin::Create(GURL("chrome://my-data-source"));
  auto other_origin = url::Origin::Create(GURL("chrome://other"));

  auto config = web_ui->GetLocalResourceLoaderConfigForTesting(
      URLDataManagerBackend::GetForBrowserContext(browser_context()),
      other_origin, /*controller=*/nullptr);

  EXPECT_FALSE(config->sources.contains(origin));
}

TEST_F(WebUIImplRenderViewHostTest,
       LocalResourceLoaderConfigWithSharedDataSource) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebUIInProcessResourceLoadingV2);
  std::unique_ptr<WebUIImpl> web_ui =
      std::make_unique<WebUIImpl>(web_contents());
  web_ui->SetRenderFrameHost(main_rfh());

  auto controller = std::make_unique<MockWebUIController>(web_ui.get());
  auto* controller_ptr = controller.get();
  web_ui->SetController(std::move(controller));

  // Add a shared data source via callback.
  controller_ptr->SetPopulateCallback(base::BindLambdaForTesting(
      [&](blink::mojom::LocalResourceLoaderConfig* config,
          const url::Origin& current_origin) {
        EXPECT_EQ(current_origin,
                  url::Origin::Create(GURL("chrome://main-ui")));
        auto source = blink::mojom::LocalResourceSource::New();
        source->replacement_strings["key"] = "value";
        config->sources[url::Origin::Create(GURL("chrome://theme"))] =
            std::move(source);
      }));

  auto origin = url::Origin::Create(GURL("chrome://theme"));

  // The shared source should be included in the config.
  auto config = WebUIImpl::GetLocalResourceLoaderConfigForTesting(
      URLDataManagerBackend::GetForBrowserContext(browser_context()),
      url::Origin::Create(GURL("chrome://main-ui")), controller_ptr);

  EXPECT_TRUE(config->sources.contains(origin));
  const auto& config_source = config->sources[origin];
  EXPECT_EQ(config_source->replacement_strings["key"], "value");
}

}  // namespace content
