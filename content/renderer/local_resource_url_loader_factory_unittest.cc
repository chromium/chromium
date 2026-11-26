// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/local_resource_url_loader_factory.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "content/common/features.h"
#include "content/common/web_ui_loading_util.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_content_client.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "net/socket/socket.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace {

// Serves registered resources while installed as the active ContentClient,
// counting direct loads vs intermediate-buffer loads.
class DirectLoadContentClient : public content::TestContentClient {
 public:
  DirectLoadContentClient()
      : old_client_(content::GetContentClientForTesting()) {
    content::SetContentClient(this);
  }
  DirectLoadContentClient(const DirectLoadContentClient&) = delete;
  DirectLoadContentClient& operator=(const DirectLoadContentClient&) = delete;
  ~DirectLoadContentClient() override {
    content::SetContentClient(old_client_);
  }

  void AddResource(int resource_id, std::string data) {
    resources_.insert_or_assign(resource_id, std::move(data));
  }

  int load_into_count() const {
    return load_into_count_.load(std::memory_order_relaxed);
  }
  int get_bytes_count() const {
    return get_bytes_count_.load(std::memory_order_relaxed);
  }

  // content::ContentClient:
  bool HasDataResource(int resource_id) const override {
    return resources_.contains(resource_id);
  }
  scoped_refptr<base::RefCountedMemory> GetDataResourceBytes(
      int resource_id) override {
    auto it = resources_.find(resource_id);
    if (it == resources_.end()) {
      return nullptr;
    }
    get_bytes_count_.fetch_add(1, std::memory_order_relaxed);
    return base::MakeRefCounted<base::RefCountedString>(it->second);
  }
  bool LoadDataResourceInto(int resource_id,
                            ResourceBufferAllocator allocate) override {
    auto it = resources_.find(resource_id);
    if (it == resources_.end() || it->second.empty()) {
      return false;
    }
    load_into_count_.fetch_add(1, std::memory_order_relaxed);
    base::span<uint8_t> buffer = allocate(it->second.size());
    if (buffer.empty()) {
      return false;
    }
    buffer.copy_from(base::as_byte_span(it->second));
    return true;
  }

 private:
  base::flat_map<int, std::string> resources_;
  std::atomic<int> load_into_count_{0};
  std::atomic<int> get_bytes_count_{0};
  raw_ptr<content::ContentClient> old_client_;
};

// A URLLoaderFactory that always sends the string "out-of-process resource" to
// the client.
class FakeURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() : receiver_(this) {}
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    // Use a SequencedTaskRunner to simulate the behavior of the
    // WebUIURLLoaderFactory, which always serves responses on a
    // SequencedTaskRunner. If this is run on the UI main thread, it will block
    // processing of mojo messages and cause deadlocks.
    auto headers = network::mojom::URLResponseHead::New();
    auto bytes =
        base::MakeRefCounted<base::RefCountedString>("out-of-process resource");
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(FROM_HERE,
                   base::BindOnce(base::IgnoreResult(&content::webui::SendData),
                                  std::move(headers), std::move(client),
                                  std::nullopt, std::move(bytes)));
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    // Supports only one receiver at a time.
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;
};

class LocalResourceURLLoaderFactoryTest : public ::testing::Test {
 public:
  LocalResourceURLLoaderFactoryTest() = default;

  void SetUp() override {
    source_ = blink::mojom::LocalResourceSource::New();
    source_->headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
            .Build()
            ->raw_headers();

    UpdateLoaderFactory();
  }

 protected:
  explicit LocalResourceURLLoaderFactoryTest(
      const base::Feature& enabled_feature)
      : feature_list_(enabled_feature),
        direct_load_content_client_(std::in_place) {}
  DirectLoadContentClient& direct_load_content_client() {
    return *direct_load_content_client_;
  }
  // Used to control the behavior of the mock ResourceBundle.
  // Marked non-private so that tests can call EXPECT_CALL on it.
  testing::NiceMock<ui::MockResourceBundleDelegate> resource_bundle_delegate_;

  content::LocalResourceURLLoaderFactory* loader_factory() {
    return loader_factory_.get();
  }

  void SetShouldReplaceI18nInJs(bool value) {
    source_->should_replace_i18n_in_js = value;
    UpdateLoaderFactory();
  }

  void AddPathToResponse(const std::string& path, const std::string& content) {
    source_->path_to_resource_map[path] =
        blink::mojom::LocalResourceValue::NewResponseBody(content);
    UpdateLoaderFactory();
  }

  void AddReplacementString(const std::string& key, const std::string& value) {
    source_->replacement_strings[key] = value;
    UpdateLoaderFactory();
  }

  void AddResourceID(const std::string& path, int id) {
    source_->path_to_resource_map[path] =
        blink::mojom::LocalResourceValue::NewResourceId(id);
    UpdateLoaderFactory();
  }

  std::string ReadAllData(network::TestURLLoaderClient& client) {
    std::string result;
    CHECK(mojo::BlockingCopyToString(client.response_body_release(), &result));
    return result;
  }

 private:
  // Create a config with a |source_| as the single hardcoded entry in the map.
  void UpdateLoaderFactory() {
    const url::Origin origin = url::Origin::Create(GURL("chrome://sourcename"));
    auto config = blink::mojom::LocalResourceLoaderConfig::New();
    config->sources[origin] = source_.Clone();
    // Create a pipe and pass receiving end to |fake_fallback_factory_|.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
    fake_fallback_factory_.Clone(
        pending_remote.InitWithNewPipeAndPassReceiver());
    // Pass other end to |loader_factory_|.
    loader_factory_ = std::make_unique<content::LocalResourceURLLoaderFactory>(
        std::move(config), std::move(pending_remote));
  }

  base::test::ScopedFeatureList feature_list_;
  std::optional<DirectLoadContentClient> direct_load_content_client_;

  std::unique_ptr<content::LocalResourceURLLoaderFactory> loader_factory_;

  FakeURLLoaderFactory fake_fallback_factory_;

  // Intermediate state that is updated by the test and eventually used to
  // update the loader factory state.
  blink::mojom::LocalResourceSourcePtr source_;

  // A ResourceBundle that uses the test's mock delegate.
  ui::ResourceBundle resource_bundle_with_mock_delegate_{
      &resource_bundle_delegate_};

  // Swap in the test ResourceBundle for the lifetime of the test.
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper_{
      &resource_bundle_with_mock_delegate_};

  // For CreateLoaderAndStart, which posts a task.
  base::test::TaskEnvironment task_environment_;
};

struct CanServeTestCase {
  std::optional<int> resource_id;
  bool has_resource;
  bool can_serve;
};

struct ServeTestCase {
  std::string path;
  std::string mime_type;
  std::string resource_data;
  std::string response_body;
};

struct RequestRangeTestCase {
  std::string request_range;
  int error_code;
  std::string resource_data;
  std::string response_body;
};

class LocalResourceURLLoaderFactoryCanServeTest
    : public LocalResourceURLLoaderFactoryTest,
      public ::testing::WithParamInterface<CanServeTestCase> {};
class LocalResourceURLLoaderFactoryServeTest
    : public LocalResourceURLLoaderFactoryTest,
      public ::testing::WithParamInterface<ServeTestCase> {};
class LocalResourceURLLoaderFactoryRequestRangeTest
    : public LocalResourceURLLoaderFactoryTest,
      public ::testing::WithParamInterface<RequestRangeTestCase> {};
class LocalResourceURLLoaderFactoryDirectLoadTest
    : public LocalResourceURLLoaderFactoryTest {
 public:
  LocalResourceURLLoaderFactoryDirectLoadTest()
      : LocalResourceURLLoaderFactoryTest(
            features::kWebUIDirectResourceDecompression) {}

 protected:
  void AddDirectResource(const std::string& path, int id, std::string data) {
    direct_load_content_client().AddResource(id, std::move(data));
    AddResourceID(path, id);
  }

  std::string LoadResource(
      const std::string& path,
      std::optional<std::string_view> range = std::nullopt) {
    network::TestURLLoaderClient client;
    network::ResourceRequest request;
    request.url = GURL("chrome://sourcename/" + path);
    if (range) {
      request.headers.SetHeader(net::HttpRequestHeaders::kRange, *range);
    }
    mojo::PendingRemote<network::mojom::URLLoader> loader;
    loader_factory()->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
        client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
    client.RunUntilComplete();

    EXPECT_EQ(net::OK, client.completion_status().error_code);
    if (!client.response_body().is_valid()) {
      ADD_FAILURE() << "No response body for " << path;
      return {};
    }
    return ReadAllData(client);
  }
};

}  // namespace

// Check if loader factory can service a particular request.
TEST_P(LocalResourceURLLoaderFactoryCanServeTest, CanServe) {
  const std::string path = "path/to/resource";
  if (GetParam().resource_id) {
    AddResourceID(path, *GetParam().resource_id);
  }
  const scoped_refptr<base::RefCountedString> resource_data =
      base::MakeRefCounted<base::RefCountedString>("in-process resource");
  ON_CALL(resource_bundle_delegate_, HasDataResource)
      .WillByDefault(testing::Return(GetParam().has_resource));
  ON_CALL(resource_bundle_delegate_, LoadDataResourceBytes)
      .WillByDefault(testing::Return(resource_data.get()));

  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  // The test fixture hardcodes the origin to 'chrome://sourcename'.
  request.url = GURL("chrome://sourcename/" + path);
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  ASSERT_TRUE(client.response_body().is_valid());
  std::string response_body = ReadAllData(client);
  if (GetParam().can_serve) {
    EXPECT_EQ(response_body, "in-process resource");
  } else {
    EXPECT_EQ(response_body, "out-of-process resource");
  }
}

INSTANTIATE_TEST_SUITE_P(
    LocalResourceURLLoaderFactoryCanServeTest,
    LocalResourceURLLoaderFactoryCanServeTest,
    ::testing::Values(
        // Resource ID exists and ResourceBundle has the resource.
        CanServeTestCase(std::make_optional(1), true, true),
        // Resource ID exists but ResourceBundle does not have the resource.
        CanServeTestCase(std::make_optional(1), false, false),
        // Resource ID does not exist in mapping.
        CanServeTestCase(std::nullopt, false, false)));

// Create loader, read bytes from the ResourceBundle and send them to the
// client.
TEST_P(LocalResourceURLLoaderFactoryServeTest, Serve) {
  const int resource_id = 1;
  const scoped_refptr<base::RefCountedString> resource_data =
      base::MakeRefCounted<base::RefCountedString>(GetParam().resource_data);
  AddResourceID(GetParam().path, resource_id);
  AddReplacementString("foo", "bar");
  SetShouldReplaceI18nInJs(true);
  // Bypass the fallback by making CanServe return true.
  ON_CALL(resource_bundle_delegate_, HasDataResource)
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(resource_bundle_delegate_,
              LoadDataResourceBytes(resource_id,
                                    ui::ResourceScaleFactor::kScaleFactorNone))
      .WillOnce(testing::Return(resource_data.get()));

  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  // The test fixture hardcodes the origin to 'chrome://sourcename'.
  request.url = GURL("chrome://sourcename/" + GetParam().path);
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_EQ(GetParam().mime_type, client.response_head()->mime_type);
  ASSERT_TRUE(client.response_body().is_valid());
  std::string response_body = ReadAllData(client);
  EXPECT_EQ(GetParam().response_body, response_body);
}

TEST_F(LocalResourceURLLoaderFactoryTest, PathToResponseMap) {
  AddPathToResponse("strings.m.js",
                    "import {loadTimeData} ... \"foo\":\"bar\"");
  // Even if replacement string is added, no replacement will be performed.
  AddReplacementString("foo", "bar");

  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = GURL("chrome://sourcename/strings.m.js");
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_EQ("text/javascript", client.response_head()->mime_type);
  ASSERT_TRUE(client.response_body().is_valid());
  std::string response_body = ReadAllData(client);
  EXPECT_THAT(response_body, testing::HasSubstr("import {loadTimeData}"));
  EXPECT_THAT(response_body, testing::HasSubstr("\"foo\":\"bar\""));
}

TEST_F(LocalResourceURLLoaderFactoryTest, QueryParametersFallBackOnMismatch) {
  AddPathToResponse("test.txt", "cached content");

  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = GURL("chrome://sourcename/test.txt?version=1");
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  std::string response_body = ReadAllData(client);
  EXPECT_EQ("out-of-process resource", response_body);
}

TEST_F(LocalResourceURLLoaderFactoryTest, QueryParametersOrderInvariant) {
  AddPathToResponse("test.txt?a=1&b=2", "cached content");

  // Request with different query parameter order should still hit the cache.
  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = GURL("chrome://sourcename/test.txt?b=2&a=1");
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  ASSERT_EQ(net::OK, client.completion_status().error_code);
  std::string response_body = ReadAllData(client);
  EXPECT_EQ("cached content", response_body);
}

INSTANTIATE_TEST_SUITE_P(
    LocalResourceURLLoaderFactoryServeTest,
    LocalResourceURLLoaderFactoryServeTest,
    ::testing::Values(
        // MIME type is assumed to be text/html. String replacement occurs.
        ServeTestCase("path/to/resource",
                      "text/html",
                      "this is $i18n{foo}",
                      "this is bar"),
        // MIME type is text/html. String replacement occurs.
        ServeTestCase("path/to/resource.html",
                      "text/html",
                      "this is $i18n{foo}",
                      "this is bar"),
        // MIME type is text/css. String replacement occurs.
        ServeTestCase("path/to/resource.css",
                      "text/css",
                      "this is $i18n{foo}",
                      "this is bar"),
        // MIME type is text/javascript. String replacement only occurs within
        // HTML template section.
        ServeTestCase("path/to/resource.js",
                      "text/javascript",
                      "this is $i18n{foo}",
                      "this is $i18n{foo}")));

// Request various byte ranges which may or may not be valid.
TEST_P(LocalResourceURLLoaderFactoryRequestRangeTest, RequestRange) {
  const std::string path = "path/to/resource";
  const int resource_id = 1;
  const scoped_refptr<base::RefCountedString> resource_data =
      base::MakeRefCounted<base::RefCountedString>(GetParam().resource_data);
  AddResourceID(path, resource_id);
  // Bypass the fallback by making CanServe return true.
  ON_CALL(resource_bundle_delegate_, HasDataResource)
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(resource_bundle_delegate_,
              LoadDataResourceBytes(resource_id,
                                    ui::ResourceScaleFactor::kScaleFactorNone))
      .WillOnce(testing::Return(resource_data.get()));

  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  // The test fixture hardcodes the origin to 'chrome://sourcename'.
  request.url = GURL("chrome://sourcename/" + path);
  request.headers.SetHeader(net::HttpRequestHeaders::kRange,
                            GetParam().request_range);
  mojo::PendingRemote<network::mojom::URLLoader> loader;
  loader_factory()->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), 0, 0, request,
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());
  client.RunUntilComplete();

  EXPECT_EQ(GetParam().error_code, client.completion_status().error_code);
  if (GetParam().error_code != net::OK) {
    return;
  }
  ASSERT_TRUE(client.response_body().is_valid());
  std::string response_body = ReadAllData(client);
  EXPECT_EQ(GetParam().response_body, response_body);
}

INSTANTIATE_TEST_SUITE_P(
    LocalResourceURLLoaderFactoryRequestRangeTest,
    LocalResourceURLLoaderFactoryRequestRangeTest,
    ::testing::Values(
        // Valid range.
        RequestRangeTestCase("bytes=3-10",
                             net::OK,
                             "resource data",
                             "ource da"),
        // Valid range, but starting byte is greater than resource size.
        // Error expected.
        RequestRangeTestCase("bytes=100-101",
                             net::ERR_REQUEST_RANGE_NOT_SATISFIABLE)));

TEST_F(LocalResourceURLLoaderFactoryDirectLoadTest, DecompressesIntoDataPipe) {
  AddDirectResource("foo.js", 1, "export const foo = 1;");
  EXPECT_EQ("export const foo = 1;", LoadResource("foo.js"));
  EXPECT_EQ(1, direct_load_content_client().load_into_count());
  EXPECT_EQ(0, direct_load_content_client().get_bytes_count());
}

TEST_F(LocalResourceURLLoaderFactoryDirectLoadTest,
       DirectLoadJsWithReplacementsWhenJsI18nDisabled) {
  std::string js =
      "_template: html`<!--_html_template_start_-->$i18n{foo}"
      "<!--_html_template_end_-->`";
  AddDirectResource("foo.js", 1, js);
  AddReplacementString("foo", "bar");

  EXPECT_EQ(js, LoadResource("foo.js"));
  EXPECT_EQ(1, direct_load_content_client().load_into_count());
  EXPECT_EQ(0, direct_load_content_client().get_bytes_count());
}

TEST_F(LocalResourceURLLoaderFactoryDirectLoadTest,
       BypassesDirectLoadForHtmlAndCssReplacements) {
  AddDirectResource("index.html", 1, "this is $i18n{foo}");
  AddDirectResource("style.css", 2, "this is $i18n{foo}");
  AddReplacementString("foo", "bar");

  EXPECT_EQ("this is bar", LoadResource("index.html"));
  EXPECT_EQ("this is bar", LoadResource("style.css"));
  EXPECT_EQ(0, direct_load_content_client().load_into_count());
  EXPECT_EQ(2, direct_load_content_client().get_bytes_count());
}

TEST_F(LocalResourceURLLoaderFactoryDirectLoadTest,
       BypassesDirectLoadForJsReplacementsWhenEnabled) {
  AddDirectResource("foo.js", 1,
                    "_template: html`<!--_html_template_start_-->$i18n{foo}"
                    "<!--_html_template_end_-->`");
  AddReplacementString("foo", "bar");
  SetShouldReplaceI18nInJs(true);

  EXPECT_EQ(
      "_template: html`<!--_html_template_start_-->bar"
      "<!--_html_template_end_-->`",
      LoadResource("foo.js"));
  EXPECT_EQ(0, direct_load_content_client().load_into_count());
  EXPECT_EQ(1, direct_load_content_client().get_bytes_count());
}

TEST_F(LocalResourceURLLoaderFactoryDirectLoadTest,
       BypassesDirectLoadForRangeRequest) {
  AddDirectResource("data.js", 1, "resource data");

  EXPECT_EQ("ource da", LoadResource("data.js", "bytes=3-10"));
  EXPECT_EQ(0, direct_load_content_client().load_into_count());
  EXPECT_EQ(1, direct_load_content_client().get_bytes_count());
}
