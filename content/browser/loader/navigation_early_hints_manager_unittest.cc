// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_early_hints_manager.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/link_header.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using PreloadedResources = NavigationEarlyHintsManager::PreloadedResources;

namespace {

const char kNavigationPath[] = "https://a.test/";
const char kPreloadPath[] = "https://a.test/script.js";
const std::string kPreloadBody = "/*empty*/";

}  // namespace

class NavigationEarlyHintsManagerTest : public testing::Test {
 public:
  NavigationEarlyHintsManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &loader_factory_)),
        early_hints_manager_(browser_context_,
                             shared_loader_factory_,
                             FrameTreeNode::kFrameTreeNodeInvalidId) {}

  ~NavigationEarlyHintsManagerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kEarlyHintsPreloadForNavigation);
  }

 protected:
  network::TestURLLoaderFactory& loader_factory() { return loader_factory_; }

  NavigationEarlyHintsManager& early_hints_manager() {
    return early_hints_manager_;
  }

  network::mojom::URLResponseHeadPtr CreatePreloadResponseHead() {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    head->headers->AddHeader("content-type", "application/javascript");
    return head;
  }

  network::mojom::EarlyHintsPtr CreateEarlyHintWithPreload() {
    auto link_header = network::mojom::LinkHeader::New(
        GURL(kPreloadPath), network::mojom::LinkRelAttribute::kPreload,
        network::mojom::LinkAsAttribute::kScript,
        network::mojom::CrossOriginAttribute::kUnspecified,
        /*mime_type=*/absl::nullopt);
    auto hints = network::mojom::EarlyHints::New();
    hints->headers = network::mojom::ParsedHeaders::New();
    hints->headers->link_headers.push_back(std::move(link_header));
    return hints;
  }

  network::ResourceRequest CreateNavigationResourceRequest() {
    network::ResourceRequest request;
    request.is_main_frame = true;
    request.url = GURL(kNavigationPath);
    return request;
  }

  PreloadedResources WaitForPreloadedResources() {
    base::RunLoop loop;
    PreloadedResources result;
    early_hints_manager().WaitForPreloadsFinishedForTesting(
        base::BindLambdaForTesting([&](PreloadedResources preloaded_resources) {
          result = preloaded_resources;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  network::TestURLLoaderFactory loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_loader_factory_;
  NavigationEarlyHintsManager early_hints_manager_;
};

TEST_F(NavigationEarlyHintsManagerTest, SimpleResponse) {
  // Set up a response which simulates coming from network.
  network::mojom::URLResponseHeadPtr head = CreatePreloadResponseHead();
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = kPreloadBody.size();
  status.error_code = net::OK;
  loader_factory().AddResponse(GURL(kPreloadPath), std::move(head),
                               kPreloadBody, status);

  early_hints_manager().HandleEarlyHints(CreateEarlyHintWithPreload(),
                                         CreateNavigationResourceRequest());

  PreloadedResources preloads = WaitForPreloadedResources();
  ASSERT_EQ(preloads.size(), 1UL);
  auto it = preloads.find(GURL(kPreloadPath));
  ASSERT_TRUE(it != preloads.end());
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
  EXPECT_FALSE(it->second.was_canceled);
}

TEST_F(NavigationEarlyHintsManagerTest, EmptyBody) {
  // Set up an empty response which simulates coming from network.
  network::mojom::URLResponseHeadPtr head = CreatePreloadResponseHead();
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = 0;
  status.error_code = net::OK;
  loader_factory().AddResponse(GURL(kPreloadPath), std::move(head), "", status);

  early_hints_manager().HandleEarlyHints(CreateEarlyHintWithPreload(),
                                         CreateNavigationResourceRequest());

  PreloadedResources preloads = WaitForPreloadedResources();
  ASSERT_EQ(preloads.size(), 1UL);
  auto it = preloads.find(GURL(kPreloadPath));
  ASSERT_TRUE(it != preloads.end());
  ASSERT_TRUE(it->second.error_code.has_value());
  EXPECT_EQ(it->second.error_code.value(), net::OK);
  EXPECT_FALSE(it->second.was_canceled);
}

TEST_F(NavigationEarlyHintsManagerTest, ResponseExistsInDiskCache) {
  // Set up a response which simulates coming from disk cache.
  network::mojom::URLResponseHeadPtr head = CreatePreloadResponseHead();
  head->was_fetched_via_cache = true;
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = kPreloadBody.size();
  status.error_code = net::OK;
  loader_factory().AddResponse(GURL(kPreloadPath), std::move(head),
                               kPreloadBody, status);

  early_hints_manager().HandleEarlyHints(CreateEarlyHintWithPreload(),
                                         CreateNavigationResourceRequest());

  PreloadedResources preloads = WaitForPreloadedResources();
  ASSERT_EQ(preloads.size(), 1UL);
  auto it = preloads.find(GURL(kPreloadPath));
  ASSERT_TRUE(it != preloads.end());
  EXPECT_TRUE(it->second.was_canceled);
}

TEST_F(NavigationEarlyHintsManagerTest, PreloadSchemeIsUnsupported) {
  auto link_header = network::mojom::LinkHeader::New(
      GURL("file:///"), network::mojom::LinkRelAttribute::kPreload,
      network::mojom::LinkAsAttribute::kUnspecified,
      network::mojom::CrossOriginAttribute::kUnspecified,
      /*mime_type=*/absl::nullopt);
  auto hints = network::mojom::EarlyHints::New();
  hints->headers = network::mojom::ParsedHeaders::New();
  hints->headers->link_headers.push_back(std::move(link_header));

  early_hints_manager().HandleEarlyHints(std::move(hints),
                                         CreateNavigationResourceRequest());

  EXPECT_TRUE(early_hints_manager().WasPreloadLinkHeaderReceived());
  EXPECT_FALSE(early_hints_manager().HasInflightPreloads());
}

}  // namespace content
