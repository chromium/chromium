// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_url_loader_factory.h"

#include <optional>

#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char* kTestWebUIScheme = kChromeUIScheme;
constexpr char kTestWebUIHost[] = "testhost";
constexpr size_t kMaxTestResourceSize = 10;

// A URLDataSource that always returns the byte sequence:
// {0, 1, 2, ..., resource_size - 1}.
class TestWebUIDataSource final : public URLDataSource {
 public:
  static std::vector<unsigned char> GetResource(size_t size) {
    std::vector<unsigned char> resource(size);
    for (size_t i = 0; i < resource.size(); ++i)
      resource[i] = i;
    return resource;
  }

  explicit TestWebUIDataSource(size_t resource_size)
      : resource_size_(resource_size) {}

  // URLDataSource implementation:
  std::string GetSource() override { return kTestWebUIHost; }

  void StartDataRequest(
      const GURL& /*url*/,
      const content::WebContents::Getter& /*wc_getter*/,
      content::URLDataSource::GotDataCallback callback) override {
    std::vector<unsigned char> raw_resource = GetResource(resource_size_);
    auto resource = base::RefCountedBytes::TakeVector(&raw_resource);
    std::move(callback).Run(std::move(resource));
  }

  std::string GetMimeType(const GURL& url) override { return "video/webm"; }

 private:
  const size_t resource_size_;
};

class OversizedWebUIDataSource final : public URLDataSource {
 public:
  explicit OversizedWebUIDataSource(size_t resource_size)
      : resource_size_(resource_size) {}

  // URLDataSource implementation:
  std::string GetSource() override { return kTestWebUIHost; }

  void StartDataRequest(
      const GURL& /*url*/,
      const content::WebContents::Getter& /*wc_getter*/,
      content::URLDataSource::GotDataCallback callback) override {
    auto resource =
        base::MakeRefCounted<OversizedRefCountedMemory>(resource_size_);
    std::move(callback).Run(std::move(resource));
  }

  std::string GetMimeType(const GURL& url) override { return "video/webm"; }

 private:
  // An implementation of RefCountedMemory representing a very large "virtual"
  // buffer that is never actually allocated.
  class OversizedRefCountedMemory final : public base::RefCountedMemory {
   public:
    explicit OversizedRefCountedMemory(size_t size) : size_(size) {}

    OversizedRefCountedMemory(const OversizedRefCountedMemory&) = delete;
    OversizedRefCountedMemory& operator=(const OversizedRefCountedMemory&) =
        delete;

    // base::RefCountedMemory implementation:
    base::span<const uint8_t> AsSpan() const override {
      // This uses `reinterpret_cast` of `1` to avoid nullness `CHECK` in the
      // constructor of `span`.
      //
      // SAFETY: This is unsound, but any use of the pointer will crash as the
      // first page is not mapped. The test does not actually use the pointer.
      return UNSAFE_BUFFERS(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(1), size_));
    }

   private:
    ~OversizedRefCountedMemory() override = default;

    const size_t size_;
  };

  const size_t resource_size_;
};

}  // namespace

const struct RangeRequestTestData {
  size_t resource_size = 0;
  std::optional<int> first_byte_position;
  std::optional<int> last_byte_position;
  int expected_error_code = net::OK;
  uint32_t expected_size = 0;
} kRangeRequestTestData[] = {
    // No range.
    {kMaxTestResourceSize, std::nullopt, std::nullopt, net::OK,
     kMaxTestResourceSize},

    // No range, 0-size resource.
    {0, std::nullopt, std::nullopt, net::OK, 0},

    {kMaxTestResourceSize, 3, std::nullopt, net::OK, kMaxTestResourceSize - 3},

    {kMaxTestResourceSize, 1, 1, net::OK, 1},

    // Range too large by 1, truncated to resource size.
    {kMaxTestResourceSize, 0, kMaxTestResourceSize, net::OK,
     kMaxTestResourceSize},

    // Range starts after the last resource byte.
    {kMaxTestResourceSize, kMaxTestResourceSize, kMaxTestResourceSize + 5,
     net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, 0},

    // Invalid range.
    {kMaxTestResourceSize, 2, 1, net::ERR_REQUEST_RANGE_NOT_SATISFIABLE, 0},

#if defined(ARCH_CPU_64_BITS)
    // Resource too large.
    {static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1,
     std::nullopt, std::nullopt, net::ERR_INSUFFICIENT_RESOURCES, 0},
#endif  // defined(ARCH_CPU_64_BITS)
};

class WebUIURLLoaderFactoryTest
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<RangeRequestTestData> {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    const auto resource_size = GetParam().resource_size;
    std::unique_ptr<URLDataSource> data_source;
    if (resource_size <= kMaxTestResourceSize)
      data_source = std::make_unique<TestWebUIDataSource>(resource_size);
    else
      data_source = std::make_unique<OversizedWebUIDataSource>(resource_size);
    URLDataManager::AddDataSource(browser_context(), std::move(data_source));
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         WebUIURLLoaderFactoryTest,
                         testing::ValuesIn(kRangeRequestTestData));

TEST_P(WebUIURLLoaderFactoryTest, RangeRequest) {
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory(
      CreateWebUIURLLoaderFactory(main_rfh(), kTestWebUIScheme,
                                  /*allowed_hosts=*/{}));

  network::ResourceRequest request;
  request.url = GURL(base::StrCat({kTestWebUIScheme, "://", kTestWebUIHost}));

  if (GetParam().first_byte_position) {
    const std::string range_value =
        GetParam().last_byte_position
            ? base::StringPrintf("bytes=%d-%d", *GetParam().first_byte_position,
                                 *GetParam().last_byte_position)
            : base::StringPrintf("bytes=%d-", *GetParam().first_byte_position);
    request.headers.SetHeader(net::HttpRequestHeaders::kRange, range_value);
  }

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient loader_client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
      /*options=*/0, request, loader_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  loader_client.RunUntilComplete();

  EXPECT_EQ(loader_client.completion_status().error_code,
            GetParam().expected_error_code);

  if (loader_client.completion_status().error_code == net::OK) {
    ASSERT_TRUE(loader_client.response_body().is_valid());
    size_t response_size;
    ASSERT_EQ(
        loader_client.response_body().ReadData(
            MOJO_READ_DATA_FLAG_QUERY, base::span<uint8_t>(), response_size),
        MOJO_RESULT_OK);
    ASSERT_EQ(response_size, GetParam().expected_size);

    if (response_size > 0u) {
      std::vector<uint8_t> response(response_size);
      ASSERT_EQ(loader_client.response_body().ReadData(
                    MOJO_READ_DATA_FLAG_ALL_OR_NONE, response, response_size),
                MOJO_RESULT_OK);

      std::vector<unsigned char> expected_resource =
          TestWebUIDataSource::GetResource(GetParam().resource_size);
      expected_resource.erase(expected_resource.begin(),
                              expected_resource.begin() +
                                  GetParam().first_byte_position.value_or(0));
      expected_resource.resize(GetParam().expected_size);
      EXPECT_EQ(response, expected_resource);
    }
  }
}

TEST(WebUIURLLoaderFactoryErrorHandlingTest, HandlesDestroyedContext) {
  content::BrowserTaskEnvironment task_environment;
  auto test_context = std::make_unique<TestBrowserContext>();
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory(
      CreateWebUIServiceWorkerLoaderFactory(test_context.get(),
                                            kTestWebUIScheme, {}));

  // Destroy the context before sending a request.
  test_context.reset();

  network::ResourceRequest request;
  request.url = GURL(base::StrCat({kTestWebUIScheme, "://", kTestWebUIHost}));

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient loader_client;
  loader_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/0,
      /*options=*/0, request, loader_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  loader_client.RunUntilComplete();

  ASSERT_EQ(loader_client.completion_status().error_code, net::ERR_FAILED);
}

}  // namespace content
