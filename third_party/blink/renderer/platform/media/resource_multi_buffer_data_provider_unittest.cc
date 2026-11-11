// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "third_party/blink/renderer/platform/media/resource_multi_buffer_data_provider.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/format_macros.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/seekable_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_reader.h"
#include "third_party/blink/renderer/platform/media/testing/mock_resource_fetch_context.h"
#include "third_party/blink/renderer/platform/media/testing/mock_web_associated_url_loader.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Truly;

const char kHttpUrl[] = "http://test";
const char kHttpRedirect[] = "http://test/ing";
const char kEtag[] = "\"arglebargle glopy-glyf?\"";

const int kDataSize = 1024;
const int kHttpOK = 200;
const int kHttpPartialContent = 206;

enum NetworkState { kNone, kLoaded, kLoading };

static bool HasIdentityAcceptEncoding(const WebURLRequest& request) {
  std::string value = request
                          .HttpHeaderField(WebString::FromUtf8(
                              net::HttpRequestHeaders::kAcceptEncoding))
                          .Utf8();
  return value.contains("identity;q=1") && value.contains("*;q=0");
}

static bool HasNoAcceptEncoding(const WebURLRequest& request) {
  return request
      .HttpHeaderField(
          WebString::FromUtf8(net::HttpRequestHeaders::kAcceptEncoding))
      .IsNull();
}

class ResourceMultiBufferDataProviderTest : public testing::Test {
 public:
  ResourceMultiBufferDataProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        url_index_(std::make_unique<UrlIndex>(
            &fetch_context_,
            0,
            task_environment_.GetMainThreadTaskRunner())) {
    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
    ON_CALL(fetch_context_, CreateUrlLoader(_))
        .WillByDefault(Invoke(
            this, &ResourceMultiBufferDataProviderTest::CreateUrlLoader));
  }

  ResourceMultiBufferDataProviderTest(
      const ResourceMultiBufferDataProviderTest&) = delete;
  ResourceMultiBufferDataProviderTest& operator=(
      const ResourceMultiBufferDataProviderTest&) = delete;

  void Initialize(const char* url,
                  int first_position,
                  media::DataSource::EncodingMode encoding_mode =
                      media::DataSource::EncodingMode::kIdentity) {
    url_ = KURL(url);
    url_data_ =
        url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED, UrlData::kNormal,
                             security_origin_, encoding_mode);
    url_data_->set_etag(kEtag);
    DCHECK(url_data_);
    url_data_->OnRedirect(
        blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                        Unretained(this)));

    first_position_ = first_position;

    auto loader = std::make_unique<ResourceMultiBufferDataProvider>(
        url_data_.get(), first_position_, /*is_client_audio_element=*/false,
        task_environment_.GetMainThreadTaskRunner());
    loader_ = loader.get();
    url_data_->multibuffer()->AddProvider(std::move(loader));
  }

  std::unique_ptr<MultiBufferReader> CreateReader(int size) {
    auto reader = std::make_unique<MultiBufferReader>(
        url_data_->multibuffer(), 0, 1024 * 1024,
        /*is_client_audio_element=*/false, base::DoNothing(),
        task_environment_.GetMainThreadTaskRunner());
    reader->SetPinRange(0, 1024 * 1024);
    reader->Wait(size, base::BindOnce([]() {}));
    return reader;
  }

  void Start() { loader_->Start(); }

  void FullResponse(int64_t instance_size, bool ok = true) {
    WebURLResponse response(url_);
    response.SetHttpHeaderField(
        WebString("Content-Length"),
        WebString::FromUtf8(base::StringPrintf("%" PRId64, instance_size)));
    response.SetExpectedContentLength(instance_size);
    response.SetHttpStatusCode(kHttpOK);
    loader_->DidReceiveResponse(response);

    if (ok) {
      EXPECT_EQ(instance_size, url_data_->length());
    }

    EXPECT_FALSE(url_data_->range_supported());
  }

  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size) {
    PartialResponse(first_position, last_position, instance_size, false, true);
  }

  void PartialResponse(int64_t first_position,
                       int64_t last_position,
                       int64_t instance_size,
                       bool chunked,
                       bool accept_ranges) {
    WebURLResponse response(url_);
    response.SetHttpHeaderField(
        WebString("Content-Range"),
        WebString::FromUtf8(
            base::StringPrintf("bytes "
                               "%" PRId64 "-%" PRId64 "/%" PRId64,
                               first_position, last_position, instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64_t content_length = -1;
    if (chunked) {
      response.SetHttpHeaderField(WebString("Transfer-Encoding"),
                                  WebString("chunked"));
    } else {
      content_length = last_position - first_position + 1;
    }
    response.SetExpectedContentLength(content_length);

    // A server isn't required to return Accept-Ranges even though it might.
    if (accept_ranges) {
      response.SetHttpHeaderField(WebString("Accept-Ranges"),
                                  WebString("bytes"));
    }

    response.SetHttpStatusCode(kHttpPartialContent);
    loader_->DidReceiveResponse(response);

    // A valid partial response should always result in this being true.
    if (url_index_) {
      EXPECT_EQ(instance_size, url_data_->length());
      EXPECT_TRUE(url_data_->range_supported());
    }
  }

  void Redirect(const char* url) {
    WebURL new_url{KURL(url)};
    WebURLResponse redirect_response(url_);

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce(
            Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));

    loader_->WillFollowRedirect(new_url, redirect_response);

    base::RunLoop().RunUntilIdle();
  }

  void StopWhenLoad() {
    loader_ = nullptr;
    url_data_ = nullptr;
  }

  MOCK_METHOD1(RedirectCallback, void(const scoped_refptr<UrlData>&));

  void SetUrlData(const scoped_refptr<UrlData>& new_url_data) {
    url_data_ = new_url_data;
  }

  void SetUrlDataAndObserveRedirects(
      const scoped_refptr<UrlData>& new_url_data) {
    url_data_ = new_url_data;
    if (url_data_) {
      url_data_->OnRedirect(blink::BindOnce(
          &ResourceMultiBufferDataProviderTest::RedirectCallback,
          Unretained(this)));
    }
  }

 protected:
  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    EXPECT_CALL(
        *url_loader,
        LoadAsynchronously(
            Truly([this](const WebURLRequest& request) {
              return expect_identity_accept_encoding_
                         ? HasIdentityAcceptEncoding(request)
                         : HasNoAcceptEncoding(request);
            }),
            loader_.get()));
    return url_loader;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  KURL url_;
  int32_t first_position_;

  NiceMock<MockResourceFetchContext> fetch_context_;
  scoped_refptr<const SecurityOrigin> security_origin_ =
      SecurityOrigin::CreateUniqueOpaque();
  std::unique_ptr<UrlIndex> url_index_;
  scoped_refptr<UrlData> url_data_;
  scoped_refptr<UrlData> redirected_to_;
  // The loader is owned by the UrlData above.
  raw_ptr<ResourceMultiBufferDataProvider> loader_;

  bool expect_identity_accept_encoding_ = false;

  std::array<uint8_t, kDataSize> data_;
};

TEST_F(ResourceMultiBufferDataProviderTest, StartStop) {
  Initialize(kHttpUrl, 0);
  Start();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       KillSwitchRestoresAcceptEncodingInjection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDeferMediaAcceptEncodingInjection);
  expect_identity_accept_encoding_ = true;

  Initialize(kHttpUrl, 0);
  Start();
  StopWhenLoad();
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(ResourceMultiBufferDataProviderTest, BadHttpResponse) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(url_);
  response.SetHttpStatusCode(404);
  response.SetHttpStatusText("Not Found\n");
  loader_->DidReceiveResponse(response);
}

TEST_F(ResourceMultiBufferDataProviderTest, DestructedUrlIndexFullResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  url_index_.reset();
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  FullResponse(1024, false);
}

TEST_F(ResourceMultiBufferDataProviderTest, DestructedUrlIndexPartialResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  url_index_.reset();
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  PartialResponse(100, 200, 1024);
}

TEST_F(ResourceMultiBufferDataProviderTest, DestructedUrlIndexDidFail) {
  Initialize(kHttpUrl, 100);
  Start();
  url_index_.reset();
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_));
}

// An ORB-blocked response is a cross-origin no-cors response, so the data must
// be marked as cross-origin to avoid leaking information about it.
TEST_F(ResourceMultiBufferDataProviderTest, DidFailBlockedByOrbIsCrossOrigin) {
  Initialize(kHttpUrl, 0);
  Start();
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_BLOCKED_BY_ORB, url_));
  EXPECT_TRUE(url_data->is_cors_cross_origin());
}

TEST_F(ResourceMultiBufferDataProviderTest, DidFailBlockedByResponse) {
  Initialize(kHttpUrl, 0);
  Start();
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_BLOCKED_BY_RESPONSE, url_));
  EXPECT_TRUE(url_data->is_cors_cross_origin());
}

TEST_F(ResourceMultiBufferDataProviderTest, DidFailOtherErrorNotCrossOrigin) {
  Initialize(kHttpUrl, 0);
  Start();
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_FAILED, url_));
  EXPECT_FALSE(url_data->is_cors_cross_origin());
}

// Deterministic security, policy, and certificate errors must fail immediately
// rather than being retried, even at a non-zero position where transient
// failures are retried.
TEST_F(ResourceMultiBufferDataProviderTest, DidFailBlockedByOrbDoesNotRetry) {
  Initialize(kHttpUrl, 100);
  // Exactly one loader must be created: the initial one. A retry would create
  // a second one. The default action from the fixture's ON_CALL applies.
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(1);
  Start();
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_BLOCKED_BY_ORB, url_));
  EXPECT_TRUE(url_data->is_cors_cross_origin());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       DidFailBlockedByOrbAfterSameOriginResponsePreservesOriginState) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  constexpr int kBlockSize = 1 << 15;

  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();

  // Initial request receives a synthetic 206 response from a service worker
  // (establishing a non-opaque `data_origin_` on `url_data_`), delivers 50
  // bytes, and finishes early, scheduling a retry from byte 50.
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 0-49/1024"));
  response.SetExpectedContentLength(50);
  response.SetWasFetchedViaServiceWorker(true);
  loader_->DidReceiveResponse(response);
  EXPECT_FALSE(url_data_->is_cors_cross_origin());

  loader_->DidReceiveData(std::string(50, 'A'));
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(1);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  // When the retry falls through to the network and is blocked by ORB before a
  // response arrives, `url_data_->is_cors_cross_origin()` must remain false
  // (matching the untainted failure when an opaque 206 response is rejected by
  // `CanAcceptDataFrom()`, b/576132668).
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(WebURLError(net::ERR_BLOCKED_BY_ORB, url_));
  EXPECT_FALSE(url_data->is_cors_cross_origin());
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       DidFailWebSecurityViolationDoesNotRetry) {
  Initialize(kHttpUrl, 100);
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(1);
  Start();
  scoped_refptr<UrlData> url_data = url_data_;
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFail(
      WebURLError(network::CorsErrorStatus(
                      network::mojom::CorsError::kWildcardOriginNotAllowed),
                  WebURLError::HasCopyInCache::kFalse, url_));
  EXPECT_TRUE(url_data->is_cors_cross_origin());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       DidFailDeterministicNetErrorDoesNotRetry) {
  for (int err :
       {net::ERR_BLOCKED_BY_CLIENT, net::ERR_BLOCKED_BY_CSP,
        net::ERR_CERT_COMMON_NAME_INVALID, net::ERR_BAD_SSL_CLIENT_AUTH_CERT}) {
    Initialize(kHttpUrl, 100);
    EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(1);
    Start();
    scoped_refptr<UrlData> url_data = url_data_;
    EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
    loader_.ExtractAsDangling()->DidFail(WebURLError(err, url_));
    EXPECT_FALSE(url_data->is_cors_cross_origin());
    task_environment_.FastForwardUntilNoTasksRemain();
  }
}

// Transient errors at a non-zero position are still retried.
TEST_F(ResourceMultiBufferDataProviderTest, DidFailOtherErrorRetries) {
  Initialize(kHttpUrl, 100);
  // The initial loader plus one retry.
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(2);
  Start();
  EXPECT_CALL(*this, RedirectCallback(_)).Times(0);
  loader_->DidFail(WebURLError(net::ERR_FAILED, url_));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ResourceMultiBufferDataProviderTest, DestructedUrlIndexDidFinish) {
  Initialize(kHttpUrl, 100);
  Start();
  FullResponse(1024, true);
  url_index_.reset();
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  loader_->DidFinishLoading();
}

// Tests that partial content is requested but not fulfilled.
TEST_F(ResourceMultiBufferDataProviderTest, NotPartialResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  FullResponse(1024, false);
}

// Tests that a 200 response is received.
TEST_F(ResourceMultiBufferDataProviderTest, FullResponse) {
  Initialize(kHttpUrl, 0);
  Start();
  FullResponse(1024);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, FullResponse_FileUrl) {
  Initialize("file://test.ogv", 0);
  Start();
  FullResponse(1024);
  StopWhenLoad();
}

// Tests that a partial content response is received.
TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse_Chunked) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, true, true);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, PartialResponse_NoAcceptRanges) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, false, false);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       PartialResponse_ChunkedNoAcceptRanges) {
  Initialize(kHttpUrl, 100);
  Start();
  PartialResponse(100, 200, 1024, true, false);
  StopWhenLoad();
}

// Tests that an invalid partial response is received.
TEST_F(ResourceMultiBufferDataProviderTest, InvalidPartialResponse) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(url_);
  response.SetHttpHeaderField(
      WebString("Content-Range"),
      WebString::FromUtf8(base::StringPrintf("bytes "
                                             "%d-%d/%d",
                                             1, 10, 1024)));
  response.SetExpectedContentLength(10);
  response.SetHttpStatusCode(kHttpPartialContent);
  loader_->DidReceiveResponse(response);
}

TEST_F(ResourceMultiBufferDataProviderTest, OpaqueRangeNotSatisfiableFails) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(url_);
  response.SetHttpStatusCode(416);
  response.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(response);
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  EXPECT_TRUE(url_data_->multibuffer()->map().empty());
}

TEST_F(ResourceMultiBufferDataProviderTest, TestRedirects) {
  // Test redirect.
  Initialize(kHttpUrl, 0);
  Start();
  Redirect(kHttpRedirect);
  FullResponse(1024);
  EXPECT_EQ(url_data_->security_origin(), security_origin_);
  StopWhenLoad();
}

// Tests partial response after a redirect.
TEST_F(ResourceMultiBufferDataProviderTest, TestRedirectedPartialResponse) {
  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(4096);
  Start();
  PartialResponse(0, 2047, 32000);
  loader_->DidReceiveData(std::string(2048, 'a'));
  Redirect(kHttpRedirect);
  PartialResponse(2048, 4096, 32000);
  EXPECT_TRUE(url_data_->Valid());
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

// This test verifies that a cross-origin redirect sequence (A -> B -> A)
// does not bypass the multibuffer security checks.
TEST_F(ResourceMultiBufferDataProviderTest, NoCrossOriginMediaLeaks) {
  // Switch to a `url_index_` with a real block size.
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA1[] = "http://localhost:18080/v1";
  constexpr char kOriginB[] = "http://127.0.0.1:18081/secret";
  constexpr char kOriginA2[] = "http://localhost:18080/v2";
  constexpr int kBlockSize = 1 << 15;

  // 1. Setup
  Initialize(kOriginA1, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  loader_->Start();

  // 2. Mark B as cross-origin (opaque response).
  WebURL url_b{KURL(kOriginB)};
  WebURLResponse response_b{url_b};
  response_b.SetHttpStatusCode(206);
  response_b.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                WebString::FromUtf8("bytes 0-1000000/1000001"));
  response_b.SetExpectedContentLength(1000001);
  response_b.SetType(network::mojom::FetchResponseType::kOpaque);

  ASSERT_TRUE(loader_->WillFollowRedirect(url_b, response_b));
  task_environment_.FastForwardUntilNoTasksRemain();

  auto set_url_data_and_create_reader =
      [&](const scoped_refptr<UrlData>& new_url_data) {
        reader.reset();
        url_data_ = new_url_data;
        if (url_data_) {
          reader = CreateReader(kBlockSize);
        }
      };

  EXPECT_CALL(*this, RedirectCallback(_))
      .WillRepeatedly(set_url_data_and_create_reader);
  loader_->DidReceiveResponse(response_b);
  ASSERT_TRUE(loader_);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(url_data_->is_cors_cross_origin());

  // 3. Receive some data for B. This data is now in the MultiBuffer for B.
  std::string data_str(kBlockSize, 'b');
  loader_->DidReceiveData(data_str);

  // 4. Simulate a failure that invokes a retry.
  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_b));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  WebURL url_a2{KURL(kOriginA2)};
  WebURLResponse response_a2{url_a2};
  response_a2.SetHttpStatusCode(206);
  response_a2.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-1000000/1000001"));
  response_a2.SetExpectedContentLength(1000001 - kBlockSize);
  response_a2.SetType(network::mojom::FetchResponseType::kBasic);

  // Redirecting back to A should fail cross origin checks.
  EXPECT_FALSE(loader_->WillFollowRedirect(url_a2, response_a2));
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

// Verifies that normal forward HTTP redirects (both cross-origin CDN redirects
// and same-origin redirects on cross-origin resources) retarget `url_data_` to
// the post-redirect URL while latching `SetSkipServiceWorker(true)` on
// follow-up range requests (preventing post-redirect URL leaks to an embedding
// page's Service Worker while preserving CDN single-use tokens), whereas
// synthetic Service Worker redirects (`Response.redirect(...)`) keep
// `GetSkipServiceWorker() == false` so offline Service Worker caches continue
// to work.
TEST_F(ResourceMultiBufferDataProviderTest,
       ForwardRedirectsRetargetAndSkipServiceWorkerExceptSyntheticSwRedirects) {
  base::test::ScopedFeatureList feature_list{
      media::kDisallowMixedOpaqueMediaResponses};
  constexpr int kBlockSize = 1 << 15;

  struct CapturedRequest {
    KURL url;
    bool skip_service_worker = false;
  };
  std::vector<CapturedRequest> requests;
  ON_CALL(fetch_context_, CreateUrlLoader(_))
      .WillByDefault([&](const WebAssociatedURLLoaderOptions&) {
        auto url_loader =
            std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
        EXPECT_CALL(*url_loader, LoadAsynchronously(_, _))
            .WillOnce([&](const WebURLRequest& request,
                          WebAssociatedURLLoaderClient*) {
              requests.push_back(
                  {request.Url(), request.GetSkipServiceWorker()});
            });
        return url_loader;
      });

  // 1. Cross-origin forward redirect (A -> CDN with token) retargets to CDN
  // and skips Service Workers on follow-up range requests.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    constexpr char kAppUrl[] = "https://app.example/media";
    constexpr char kCdnUrl[] = "https://cdn.example/edge/clip.mp4?token=once";
    security_origin_ = SecurityOrigin::CreateFromString(kAppUrl);

    Initialize(kAppUrl, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    requests.clear();
    loader_->Start();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests.back().url, KURL(kAppUrl));
    EXPECT_FALSE(requests.back().skip_service_worker);

    WebURL cdn_url{KURL(kCdnUrl)};
    WebURLResponse redir{KURL(kAppUrl)};
    redir.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(cdn_url, redir));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlData(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{cdn_url};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/131072"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kOpaque);
    loader_->DidReceiveResponse(response1);

    EXPECT_EQ(url_data_->url(), KURL(kCdnUrl));
    EXPECT_EQ(url_data_->service_worker_mode(),
              UrlData::ServiceWorkerMode::kSkip);

    std::string data_str(kBlockSize, 'a');
    loader_->DidReceiveData(data_str);
    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();

    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests.back().url, KURL(kCdnUrl));
    EXPECT_TRUE(requests.back().skip_service_worker);

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }

  // 2. Same-origin redirect on a cross-origin resource (`victim/api ->
  // victim/secret?sig=123` embedded by `attacker.example`) also retargets and
  // sets `SetSkipServiceWorker(true)` so `attacker.example`'s Service Worker
  // never observes `victim/secret?sig=123`.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    constexpr char kAttackerPage[] = "https://attacker.example/page.html";
    constexpr char kVictimInitialUrl[] = "https://victim.example/api/latest";
    constexpr char kVictimSecretUrl[] =
        "https://victim.example/files/secret.wav?sig=123";
    security_origin_ = SecurityOrigin::CreateFromString(kAttackerPage);

    Initialize(kVictimInitialUrl, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    requests.clear();
    loader_->Start();

    WebURL secret_url{KURL(kVictimSecretUrl)};
    WebURLResponse redir{KURL(kVictimInitialUrl)};
    redir.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(secret_url, redir));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlData(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{secret_url};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/131072"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kOpaque);
    loader_->DidReceiveResponse(response1);

    EXPECT_EQ(url_data_->url(), KURL(kVictimSecretUrl));
    EXPECT_EQ(url_data_->service_worker_mode(),
              UrlData::ServiceWorkerMode::kSkip);

    std::string data_str(kBlockSize, 'a');
    loader_->DidReceiveData(data_str);
    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();

    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests.back().url, KURL(kVictimSecretUrl));
    EXPECT_TRUE(requests.back().skip_service_worker);

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }

  // 3. Synthetic Service Worker redirect (`WasFetchedViaServiceWorker() ==
  // true`) keeps `ServiceWorkerMode::kAllow` (`skip_service_worker == false`)
  // so offline Service Worker caches can serve follow-up range requests.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    constexpr char kOnlineUrl[] = "https://app.example/video.mp4";
    constexpr char kOfflineCachedUrl[] =
        "https://app.example/offline/video.mp4";
    security_origin_ = SecurityOrigin::CreateFromString(kOnlineUrl);

    Initialize(kOnlineUrl, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    requests.clear();
    loader_->Start();

    WebURL offline_url{KURL(kOfflineCachedUrl)};
    WebURLResponse sw_redir{KURL(kOnlineUrl)};
    sw_redir.SetHttpStatusCode(302);
    sw_redir.SetWasFetchedViaServiceWorker(true);
    ASSERT_TRUE(loader_->WillFollowRedirect(offline_url, sw_redir));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlData(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{offline_url};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/131072"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kBasic);
    response1.SetWasFetchedViaServiceWorker(true);
    response1.SetUrlListViaServiceWorker(std::vector<WebURL>{offline_url});
    loader_->DidReceiveResponse(response1);

    EXPECT_EQ(url_data_->url(), KURL(kOfflineCachedUrl));
    EXPECT_EQ(url_data_->service_worker_mode(),
              UrlData::ServiceWorkerMode::kAllow);

    auto seek_writer = url_data_->multibuffer()->CreateWriter(
        /*pos=*/2, /*is_client_audio_element=*/false);
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests.back().url, KURL(kOfflineCachedUrl));
    EXPECT_FALSE(requests.back().skip_service_worker);
    seek_writer.reset();

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }

  // 4. Disabling `kDisallowMixedOpaqueMediaResponses` restores
  // `ServiceWorkerMode::kAllow` (`skip_service_worker == false`) across network
  // redirects.
  {
    base::test::ScopedFeatureList disable_mixed_opaque;
    disable_mixed_opaque.InitAndDisableFeature(
        media::kDisallowMixedOpaqueMediaResponses);

    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    constexpr char kAppUrl[] = "https://app.example/media";
    constexpr char kCdnUrl[] = "https://cdn.example/edge/clip.mp4";
    security_origin_ = SecurityOrigin::CreateFromString(kAppUrl);

    Initialize(kAppUrl, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    requests.clear();
    loader_->Start();

    WebURL cdn_url{KURL(kCdnUrl)};
    WebURLResponse redir{KURL(kAppUrl)};
    redir.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(cdn_url, redir));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlData(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{cdn_url};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/131072"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kOpaque);
    loader_->DidReceiveResponse(response1);

    EXPECT_EQ(url_data_->url(), KURL(kCdnUrl));
    EXPECT_EQ(url_data_->service_worker_mode(),
              UrlData::ServiceWorkerMode::kAllow);

    auto seek_writer = url_data_->multibuffer()->CreateWriter(
        /*pos=*/2, /*is_client_audio_element=*/false);
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests.back().url, KURL(kCdnUrl));
    EXPECT_FALSE(requests.back().skip_service_worker);
    seek_writer.reset();

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }
}

// Verifies that when a same-origin URL (A) is already cached in `UrlIndex`, a
// cross-origin bounce-back (C -> A, including `kSameSite -> kSameOrigin` as in
// WPT `no-cors.https.html`) creates an unindexed `UrlData` (with `url() == A`
// and `request_url() == C`) instead of colliding with or tainting A's
// non-opaque `UrlData` in `UrlIndex`.
TEST_F(ResourceMultiBufferDataProviderTest,
       CrossOriginBounceBackToCachedUrlCreatesUnindexedUrlData) {
  for (const char* bounce_start : {"https://attacker.example/redir",
                                   "https://victim.example:8445/redir"}) {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());

    constexpr char kOriginA[] = "https://victim.example:8444/4ch-440.wav";
    security_origin_ = SecurityOrigin::Create(KURL(kOriginA));

    // 1. Load `kOriginA` directly as a non-opaque response so it is cached in
    // `url_index_`.
    Initialize(kOriginA, 0);
    url_data_->Use();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURL url_a{KURL(kOriginA)};
    WebURLResponse response_a{url_a};
    response_a.SetHttpStatusCode(kHttpPartialContent);
    response_a.SetHttpHeaderField(
        WebString::FromUtf8("Content-Range"),
        WebString::FromUtf8("bytes 0-1000000/1000001"));
    response_a.SetExpectedContentLength(1000001);
    response_a.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response_a);

    scoped_refptr<UrlData> url_data_a = url_data_;
    ASSERT_EQ(url_index_->GetByUrl(KURL(kOriginA), UrlData::CORS_UNSPECIFIED,
                                   UrlData::kNormal, security_origin_),
              url_data_a);
    EXPECT_FALSE(url_data_a->is_cors_cross_origin());

    // 2. Load `bounce_start`, which redirects cross-origin back to `kOriginA`.
    // It must create a separate unindexed `UrlData` rather than failing or
    // merging onto `url_data_a`.
    Initialize(bounce_start, 0);
    url_data_->Use();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURLResponse redirect_response{KURL(bounce_start)};
    ASSERT_TRUE(loader_->WillFollowRedirect(url_a, redirect_response));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce(
            Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));
    WebURLResponse response_c{url_a};
    response_c.SetHttpStatusCode(kHttpPartialContent);
    response_c.SetHttpHeaderField(
        WebString::FromUtf8("Content-Range"),
        WebString::FromUtf8("bytes 0-1000000/1000001"));
    response_c.SetExpectedContentLength(1000001);
    response_c.SetType(network::mojom::FetchResponseType::kOpaque);
    loader_->DidReceiveResponse(response_c);

    ASSERT_TRUE(url_data_);
    EXPECT_NE(url_data_, url_data_a);
    EXPECT_EQ(url_data_->url(), KURL(kOriginA));
    EXPECT_EQ(url_data_->request_url(), KURL(bounce_start));
    EXPECT_TRUE(url_data_->has_original_request_url());
    EXPECT_EQ(url_data_->data_origin(), KURL(kOriginA));
    EXPECT_TRUE(url_data_->is_cors_cross_origin());
    EXPECT_TRUE(url_data_->has_cross_origin_redirect());

    // `url_data_a` remains indexed for `kOriginA` (untainted), while the
    // bounce-back `url_data_` is unindexed under both `kOriginA` and
    // `bounce_start`.
    EXPECT_EQ(url_index_->GetByUrl(KURL(kOriginA), UrlData::CORS_UNSPECIFIED,
                                   UrlData::kNormal, security_origin_),
              url_data_a);
    EXPECT_FALSE(url_data_a->is_cors_cross_origin());
    EXPECT_NE(
        url_index_->GetByUrl(KURL(bounce_start), UrlData::CORS_UNSPECIFIED,
                             UrlData::kNormal, security_origin_),
        url_data_);
    StopWhenLoad();
  }
}

// Verifies that a cross-site -> same-origin/same-site bounce-back redirect
// preserves `original_request_url_` so all follow-up range requests (short-read
// retries, error retries, and new writers for seeks) replay the original
// pre-redirect URL (`Sec-Fetch-Site` and `SameSite` cookies) rather than
// issuing direct requests to the post-redirect target.
TEST_F(ResourceMultiBufferDataProviderTest,
       CrossOriginBounceBackReplaysOriginalUrlOnFollowUpRequests) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kEmbeddingPage[] = "https://victim.example/page.html";
  constexpr char kAttackerUrl[] = "https://attacker.example/clip";
  constexpr char kVictimSubdomainUrl[] =
      "https://sub.victim.example/api/action?p=evil";
  constexpr int kBlockSize = 1 << 15;

  security_origin_ = SecurityOrigin::CreateFromString(kEmbeddingPage);

  std::vector<KURL> requested_urls;
  ON_CALL(fetch_context_, CreateUrlLoader(_))
      .WillByDefault([&](const WebAssociatedURLLoaderOptions&) {
        auto url_loader =
            std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
        EXPECT_CALL(*url_loader, LoadAsynchronously(_, _))
            .WillOnce([&](const WebURLRequest& request,
                          WebAssociatedURLLoaderClient*) {
              requested_urls.push_back(request.Url());
            });
        return url_loader;
      });

  Initialize(kAttackerUrl, 0);
  auto reader = CreateReader(kBlockSize * 2);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  requested_urls.clear();
  loader_->Start();
  ASSERT_EQ(requested_urls.size(), 1u);
  EXPECT_EQ(requested_urls.back(), KURL(kAttackerUrl));

  // Cross-site -> same-site bounce-back (`kCrossSite` -> `kSameSite` relative
  // to `victim.example`). Even if the mock response type is `kBasic`, the
  // cross-origin bounce taints the resource and keeps `request_url()` at
  // `kAttackerUrl` while reporting `url()` as `kVictimSubdomainUrl`.
  WebURL victim_url{KURL(kVictimSubdomainUrl)};
  WebURLResponse redirect_response{KURL(kAttackerUrl)};
  redirect_response.SetHttpStatusCode(302);
  ASSERT_TRUE(loader_->WillFollowRedirect(victim_url, redirect_response));

  EXPECT_CALL(*this, RedirectCallback(_))
      .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
        reader.reset();
        SetUrlData(new_url_data);
        reader = CreateReader(kBlockSize * 2);
      });
  WebURLResponse response1{victim_url};
  response1.SetHttpStatusCode(kHttpPartialContent);
  response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-32767/131072"));
  response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                               WebString::FromUtf8("bytes"));
  response1.SetExpectedContentLength(kBlockSize);
  response1.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response1);
  ASSERT_TRUE(loader_);
  EXPECT_EQ(url_data_->url(), KURL(kVictimSubdomainUrl));
  EXPECT_EQ(url_data_->request_url(), KURL(kAttackerUrl));
  ASSERT_TRUE(url_data_->data_origin().has_value());
  EXPECT_EQ(*url_data_->data_origin(), KURL(kVictimSubdomainUrl));
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  EXPECT_TRUE(url_data_->has_cross_origin_redirect());

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  // 1. Short-read retry via DidFinishLoading() before the full resource length
  // is received must re-request `kAttackerUrl`, not `kVictimSubdomainUrl`.
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(requested_urls.size(), 2u);
  EXPECT_EQ(requested_urls.back(), KURL(kAttackerUrl));

  // Re-traversing the same cross-origin bounce to `kVictimSubdomainUrl`
  // succeeds in-place without allocating another `UrlData`.
  EXPECT_CALL(*this, RedirectCallback(_)).Times(0);
  ASSERT_TRUE(loader_->WillFollowRedirect(victim_url, redirect_response));
  WebURLResponse response2{victim_url};
  response2.SetHttpStatusCode(kHttpPartialContent);
  response2.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 32768-65535/131072"));
  response2.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                               WebString::FromUtf8("bytes"));
  response2.SetExpectedContentLength(kBlockSize);
  response2.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response2);
  loader_->DidReceiveData(data_str);

  // 2. Transient network failure retry via DidFail() at pos_ != 0 must also
  // re-request `kAttackerUrl`.
  loader_->DidFail(WebURLError(net::ERR_FAILED, victim_url));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(requested_urls.size(), 3u);
  EXPECT_EQ(requested_urls.back(), KURL(kAttackerUrl));

  // 3. A new range request writer (e.g. from seeking) must also request
  // `kAttackerUrl`.
  auto seek_writer = url_data_->multibuffer()->CreateWriter(
      /*pos=*/3, /*is_client_audio_element=*/false);
  ASSERT_EQ(requested_urls.size(), 4u);
  EXPECT_EQ(requested_urls.back(), KURL(kAttackerUrl));
  seek_writer.reset();

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

// Verifies that a multi-hop bounce-back chain A1 -> B -> A2 is kept unindexed
// (not merged into cached A2), marked cross-origin, replays A1 on follow-up
// range requests, and rejects a retry that stops at B.
TEST_F(ResourceMultiBufferDataProviderTest,
       MultiHopCrossOriginBounceToSameOriginDoesNotMergeAndRejectsMismatch) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA1[] = "https://victim.example/start";
  constexpr char kOriginB[] = "https://attacker.example/bounce";
  constexpr char kOriginA2[] = "https://victim.example/target";
  constexpr int kBlockSize = 1 << 15;

  security_origin_ = SecurityOrigin::Create(KURL(kOriginA1));

  // Pre-populate `url_index_` with a non-opaque `UrlData` for `kOriginA2`.
  scoped_refptr<UrlData> cached_a2 =
      url_index_->GetByUrl(KURL(kOriginA2), UrlData::CORS_UNSPECIFIED,
                           UrlData::kNormal, security_origin_);
  cached_a2->Use();
  cached_a2->set_range_supported();
  cached_a2->SetDataOrigin(KURL(kOriginA2), /*is_cors_cross_origin=*/false);
  ASSERT_EQ(cached_a2, url_index_->TryInsert(cached_a2));

  Initialize(kOriginA1, 0);
  auto reader = CreateReader(kBlockSize * 2);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  WebURL url_b{KURL(kOriginB)};
  WebURLResponse redirect_to_b{KURL(kOriginA1)};
  redirect_to_b.SetHttpStatusCode(302);
  ASSERT_TRUE(loader_->WillFollowRedirect(url_b, redirect_to_b));
  // `WillFollowRedirect` must not mutate `url_data_` before
  // `DidReceiveResponse` validates the response.
  EXPECT_FALSE(url_data_->has_cross_origin_redirect());

  WebURL url_a2{KURL(kOriginA2)};
  WebURLResponse redirect_to_a2{url_b};
  redirect_to_a2.SetHttpStatusCode(302);
  ASSERT_TRUE(loader_->WillFollowRedirect(url_a2, redirect_to_a2));

  WebURLResponse response_a2{url_a2};
  response_a2.SetHttpStatusCode(kHttpPartialContent);
  response_a2.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/100000"));
  response_a2.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
  response_a2.SetExpectedContentLength(kBlockSize);
  response_a2.SetType(network::mojom::FetchResponseType::kBasic);

  EXPECT_CALL(*this, RedirectCallback(_))
      .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
        reader.reset();
        SetUrlDataAndObserveRedirects(new_url_data);
        reader = CreateReader(kBlockSize * 2);
      });
  loader_->DidReceiveResponse(response_a2);
  ASSERT_TRUE(loader_);
  EXPECT_EQ(url_data_->url(), KURL(kOriginA2));
  EXPECT_EQ(url_data_->request_url(), KURL(kOriginA1));
  EXPECT_NE(url_data_, cached_a2);
  EXPECT_FALSE(cached_a2->is_cors_cross_origin());
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  EXPECT_TRUE(url_data_->has_cross_origin_redirect());

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  // A retry that stops at `kOriginB` instead of continuing to `kOriginA2` must
  // be rejected because its origin does not match `data_origin()`
  // (`kOriginA2`).
  loader_->DidFail(WebURLError(net::ERR_FAILED, url_a2));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  ASSERT_TRUE(loader_->WillFollowRedirect(url_b, redirect_to_b));
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  WebURLResponse response_b{url_b};
  response_b.SetHttpStatusCode(kHttpPartialContent);
  response_b.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/100000"));
  response_b.SetExpectedContentLength(kBlockSize);
  response_b.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(response_b);

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

// Verifies that when a redirect hop fails Timing-Allow-Origin
// (`TimingAllowPassed == false`) before retargeting onto a CDN URL whose direct
// follow-up range responses pass TAO (`TimingAllowPassed == true`), `UrlData`
// latches the TAO failure rather than upgrading back to true on follow-up range
// requests, while a rejected HTTP error response does not poison TAO on an
// already-passing cached `UrlData`.
TEST_F(ResourceMultiBufferDataProviderTest,
       TimingAllowOriginFailureLatchesAcrossRetargetedRangeRequests) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kRouterUrl[] = "https://router.example/clip";
  constexpr char kCdnUrl[] = "https://cdn.example/clip.mp4";
  constexpr int kBlockSize = 1 << 15;

  security_origin_ = SecurityOrigin::CreateFromString("https://app.example");
  url_ = KURL(kRouterUrl);
  url_data_ = url_index_->GetByUrl(url_, UrlData::CORS_ANONYMOUS,
                                   UrlData::kNormal, security_origin_);
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  auto loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), 0, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  loader_ = loader.get();
  url_data_->multibuffer()->AddProvider(std::move(loader));
  auto reader = CreateReader(kBlockSize * 2);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  WebURL cdn_url{KURL(kCdnUrl)};
  WebURLResponse redir{url_};
  redir.SetHttpStatusCode(302);
  ASSERT_TRUE(loader_->WillFollowRedirect(cdn_url, redir));

  EXPECT_CALL(*this, RedirectCallback(_))
      .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
        reader.reset();
        SetUrlData(new_url_data);
        reader = CreateReader(kBlockSize * 2);
      });
  WebURLResponse response1{cdn_url};
  response1.SetHttpStatusCode(kHttpPartialContent);
  response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-32767/65536"));
  response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                               WebString::FromUtf8("bytes"));
  response1.SetExpectedContentLength(kBlockSize);
  response1.SetType(network::mojom::FetchResponseType::kCors);
  response1.SetTimingAllowPassed(false);
  loader_->DidReceiveResponse(response1);
  ASSERT_TRUE(loader_);
  EXPECT_EQ(url_data_->url(), KURL(kCdnUrl));
  EXPECT_FALSE(url_data_->passed_timing_allow_origin_check());

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  // Direct follow-up range request to `kCdnUrl` returns
  // `TimingAllowPassed() == true`, which must not clear the initial redirect's
  // TAO failure.
  WebURLResponse response2{cdn_url};
  response2.SetHttpStatusCode(kHttpPartialContent);
  response2.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 32768-65535/65536"));
  response2.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                               WebString::FromUtf8("bytes"));
  response2.SetExpectedContentLength(kBlockSize);
  response2.SetType(network::mojom::FetchResponseType::kCors);
  response2.SetTimingAllowPassed(true);
  loader_->DidReceiveResponse(response2);
  EXPECT_FALSE(url_data_->passed_timing_allow_origin_check());

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();

  // Conversely, if a cached `UrlData` already passed TAO, a failed follow-up
  // range response (e.g. HTTP 503 with `TimingAllowPassed() == false`) must not
  // latch TAO failure onto the shared cache entry.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    url_ = KURL(kCdnUrl);
    url_data_ = url_index_->GetByUrl(url_, UrlData::CORS_ANONYMOUS,
                                     UrlData::kNormal, security_origin_);
    url_data_->OnRedirect(
        blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                        Unretained(this)));
    auto ok_loader = std::make_unique<ResourceMultiBufferDataProvider>(
        url_data_.get(), 0, /*is_client_audio_element=*/false,
        task_environment_.GetMainThreadTaskRunner());
    loader_ = ok_loader.get();
    url_data_->multibuffer()->AddProvider(std::move(ok_loader));
    auto ok_reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURLResponse ok_response{cdn_url};
    ok_response.SetHttpStatusCode(kHttpPartialContent);
    ok_response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                   WebString::FromUtf8("bytes 0-32767/65536"));
    ok_response.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                   WebString::FromUtf8("bytes"));
    ok_response.SetExpectedContentLength(kBlockSize);
    ok_response.SetType(network::mojom::FetchResponseType::kCors);
    ok_response.SetTimingAllowPassed(true);
    loader_->DidReceiveResponse(ok_response);
    EXPECT_TRUE(url_data_->passed_timing_allow_origin_check());

    loader_->DidReceiveData(data_str);
    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);

    EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
    WebURLResponse error_response{cdn_url};
    error_response.SetHttpStatusCode(503);
    error_response.SetType(network::mojom::FetchResponseType::kCors);
    error_response.SetTimingAllowPassed(false);
    scoped_refptr<UrlData> shared_entry = url_data_;
    loader_->DidReceiveResponse(error_response);
    EXPECT_TRUE(shared_entry->passed_timing_allow_origin_check());

    loader_ = nullptr;
    ok_reader.reset();
    StopWhenLoad();
  }
}

// Verifies that if the initial response did not follow a cross-origin redirect,
// a subsequent range request that follows a cross-origin redirect is rejected
// immediately in WillFollowRedirect() rather than sending the redirected
// request or mutating `has_cross_origin_redirect()` on the existing `UrlData`.
TEST_F(ResourceMultiBufferDataProviderTest,
       CrossOriginRedirectOnSubsequentRangeFailsWithoutMutatingUrlData) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA[] = "https://victim.example/video.mp4";
  constexpr char kOriginB[] = "https://other.example/video.mp4";
  constexpr int kBlockSize = 1 << 15;

  url_ = KURL(kOriginA);
  security_origin_ = SecurityOrigin::Create(url_);
  url_data_ = url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED,
                                   UrlData::kNormal, security_origin_);
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  auto loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), 0, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  loader_ = loader.get();
  url_data_->multibuffer()->AddProvider(std::move(loader));
  auto reader = CreateReader(kBlockSize * 2);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  // 1. Initial range response is direct (no cross-origin redirect).
  WebURLResponse response1{url_};
  response1.SetHttpStatusCode(kHttpPartialContent);
  response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-32767/65536"));
  response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                               WebString::FromUtf8("bytes"));
  response1.SetExpectedContentLength(kBlockSize);
  response1.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response1);
  ASSERT_TRUE(loader_);
  EXPECT_FALSE(url_data_->has_cross_origin_redirect());

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  // 2. Follow-up range request redirects cross-origin; WillFollowRedirect must
  // fail immediately without mutating `has_cross_origin_redirect()` on
  // `url_data_`.
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  WebURL url_b{KURL(kOriginB)};
  WebURLResponse redirect_response{url_};
  redirect_response.SetHttpStatusCode(302);
  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
  EXPECT_FALSE(loader_->WillFollowRedirect(url_b, redirect_response));
  EXPECT_FALSE(url_data_->has_cross_origin_redirect());

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

// Verifies that when a same-origin URL (A) is already cached in `UrlIndex` as a
// non-opaque (`kBasic`) response, a second cross-origin URL (C) redirecting to
// A (which yields an opaque response) succeeds rather than failing playback.
TEST_F(ResourceMultiBufferDataProviderTest,
       CrossOriginRedirectToCachedUrlSucceeds) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA[] = "http://localhost:18080/v1";
  constexpr char kOriginC[] = "http://127.0.0.1:18081/redir";

  // 1. Load `kOriginA` directly as a non-opaque response so it is cached in
  // `url_index_`.
  Initialize(kOriginA, 0);
  url_data_->Use();
  ASSERT_TRUE(loader_);
  loader_->Start();

  WebURL url_a{KURL(kOriginA)};
  WebURLResponse response_a{url_a};
  response_a.SetHttpStatusCode(kHttpPartialContent);
  response_a.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                WebString::FromUtf8("bytes 0-1000000/1000001"));
  response_a.SetExpectedContentLength(1000001);
  response_a.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response_a);

  scoped_refptr<UrlData> url_data_a = url_data_;
  ASSERT_EQ(url_index_->GetByUrl(KURL(kOriginA), UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_),
            url_data_a);
  EXPECT_FALSE(url_data_a->is_cors_cross_origin());

  // 2. Load `kOriginC`, which redirects cross-origin to `kOriginA` with an
  // opaque response. It must succeed (redirecting to a non-null `UrlData`)
  // rather than failing (`RedirectCallback(nullptr)`).
  EXPECT_CALL(*this, RedirectCallback(testing::NotNull()))
      .WillOnce(Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));

  Initialize(kOriginC, 0);
  url_data_->Use();
  ASSERT_TRUE(loader_);
  loader_->Start();

  WebURLResponse redirect_response{KURL(kOriginC)};
  ASSERT_TRUE(loader_->WillFollowRedirect(url_a, redirect_response));

  WebURLResponse response_c{url_a};
  response_c.SetHttpStatusCode(kHttpPartialContent);
  response_c.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                WebString::FromUtf8("bytes 0-1000000/1000001"));
  response_c.SetExpectedContentLength(1000001);
  response_c.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(response_c);

  ASSERT_TRUE(url_data_);
  EXPECT_EQ(url_data_->data_origin(), KURL(kOriginA));
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  StopWhenLoad();
}

// Verifies that a same-origin redirect cannot be used to mix responses with
// different data origins (e.g., via a Service Worker responding to the
// redirected URL with a cross-origin response).
TEST_F(ResourceMultiBufferDataProviderTest,
       SameOriginRedirectRejectsMismatchedDataOrigin) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA1[] = "http://localhost:18080/v1";
  constexpr char kOriginA2[] = "http://localhost:18080/v2";
  constexpr char kOriginB[] = "http://127.0.0.1:18081/secret";
  constexpr int kBlockSize = 1 << 15;

  Initialize(kOriginA1, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  // 1. Receive initial partial response from Origin A.
  WebURL url_a1{KURL(kOriginA1)};
  WebURLResponse response_a1{url_a1};
  response_a1.SetHttpStatusCode(kHttpPartialContent);
  response_a1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/100000"));
  response_a1.SetExpectedContentLength(kBlockSize);
  response_a1.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response_a1);
  ASSERT_TRUE(loader_);

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  // 2. Simulate retry/next range on the same loader redirecting same-origin to
  // kOriginA2, which WillFollowRedirect allows because A1 and A2 are
  // same-origin.
  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_a1));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  WebURL url_a2{KURL(kOriginA2)};
  WebURLResponse redirect_response{url_a1};
  ASSERT_TRUE(loader_->WillFollowRedirect(url_a2, redirect_response));

  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));

  // 3. Service Worker responds to A2 with a response from Origin B.
  WebURL url_b{KURL(kOriginB)};
  WebURLResponse response_b{url_b};
  response_b.SetHttpStatusCode(kHttpPartialContent);
  response_b.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/100000"));
  response_b.SetExpectedContentLength(kBlockSize);
  response_b.SetType(network::mojom::FetchResponseType::kBasic);
  response_b.SetWasFetchedViaServiceWorker(true);
  response_b.SetUrlListViaServiceWorker(std::vector<WebURL>{url_b});

  loader_->DidReceiveResponse(response_b);
  StopWhenLoad();
}

// Verifies that a response rejected by CanAcceptDataFrom() does not overwrite
// metadata (such as `length()`) on the shared `UrlData` in `UrlIndex`.
TEST_F(ResourceMultiBufferDataProviderTest,
       MismatchedDataOriginDoesNotMutateUrlData) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA[] = "http://localhost:18080/v1";
  constexpr char kOriginB[] = "http://127.0.0.1:18081/secret";
  constexpr int kBlockSize = 1 << 15;
  constexpr int64_t kInitialLength = 100000;
  constexpr int64_t kSecretLength = 424242;

  Initialize(kOriginA, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  // 1. Receive initial partial response from Origin A, pinning `data_origin_`
  // and `length()` on `url_data_` in `url_index_`.
  WebURL url_a{KURL(kOriginA)};
  WebURLResponse response_a{url_a};
  response_a.SetHttpStatusCode(kHttpPartialContent);
  response_a.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                WebString::FromUtf8("bytes 0-32767/100000"));
  response_a.SetExpectedContentLength(kBlockSize);
  response_a.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response_a);
  ASSERT_TRUE(loader_);
  EXPECT_EQ(url_data_->length(), kInitialLength);

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  // 2. Simulate retry/next range on the same `url_data_` where a Service Worker
  // responds with a 200 OK response from Origin B carrying a different
  // Content-Length.
  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_a));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));

  WebURL url_b{KURL(kOriginB)};
  WebURLResponse response_b{url_b};
  response_b.SetHttpStatusCode(kHttpOK);
  response_b.SetExpectedContentLength(kSecretLength);
  response_b.SetType(network::mojom::FetchResponseType::kOpaque);
  response_b.SetWasFetchedViaServiceWorker(true);
  response_b.SetUrlListViaServiceWorker(std::vector<WebURL>{url_b});

  loader_->DidReceiveResponse(response_b);
  EXPECT_EQ(url_data_->length(), kInitialLength);
  EXPECT_FALSE(url_data_->is_cors_cross_origin());
  StopWhenLoad();
}

// Verifies that a subsequent opaque response on a non-opaque resource (even
// from the same data origin) is rejected before mutating metadata on the shared
// `UrlData`.
TEST_F(ResourceMultiBufferDataProviderTest,
       MixedOpaqueResponseDoesNotMutateUrlData) {
  base::test::ScopedFeatureList feature_list(
      media::kDisallowMixedOpaqueMediaResponses);
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());

  constexpr char kOriginA[] = "http://localhost:18080/v1";
  constexpr int kBlockSize = 1 << 15;
  constexpr int64_t kInitialLength = 100000;
  constexpr int64_t kNewLength = 424242;

  Initialize(kOriginA, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  loader_->Start();

  WebURL url_a{KURL(kOriginA)};
  WebURLResponse response1{url_a};
  response1.SetHttpStatusCode(kHttpPartialContent);
  response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-32767/100000"));
  response1.SetExpectedContentLength(kBlockSize);
  response1.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response1);
  ASSERT_TRUE(loader_);
  EXPECT_EQ(url_data_->length(), kInitialLength);
  EXPECT_FALSE(url_data_->is_cors_cross_origin());

  std::string data_str(kBlockSize, 'a');
  loader_->DidReceiveData(data_str);

  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_a));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);

  EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));

  WebURLResponse response2{url_a};
  response2.SetHttpStatusCode(kHttpOK);
  response2.SetExpectedContentLength(kNewLength);
  response2.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(response2);
  EXPECT_EQ(url_data_->length(), kInitialLength);
  EXPECT_FALSE(url_data_->is_cors_cross_origin());
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, AllowGzip) {
  Initialize(kHttpUrl, 0, media::DataSource::EncodingMode::kAllowGzip);

  auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
  EXPECT_CALL(*url_loader,
              LoadAsynchronously(
                  Truly([](const WebURLRequest& request) {
                    std::string value =
                        request
                            .HttpHeaderField(WebString::FromUtf8(
                                net::HttpRequestHeaders::kAcceptEncoding))
                            .Utf8();
                    return value.contains("gzip");
                  }),
                  loader_.get()));

  EXPECT_CALL(fetch_context_, CreateUrlLoader(_))
      .WillOnce(testing::Return(testing::ByMove(std::move(url_loader))));

  Start();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       PruneEmptiesUrlDataDuringDidReceiveData) {
  Initialize(kHttpUrl, 0);
  url_data_->Use();
  Start();
  PartialResponse(0, 100, 1024);

  // Drop the test's reference so `url_index_` holds the sole reference to
  // `UrlData`. Since there is no active `MultiBufferReader`, `GlobalLRU` max
  // size is 0 and blocks are unpinned. Receiving a full block will cause
  // `OnDataProviderEvent` to prune all blocks and invoke `UrlData::OnEmpty()`,
  // which must not destroy `UrlData` / `MultiBuffer` while still on the stack.
  ResourceMultiBufferDataProvider* loader = loader_.ExtractAsDangling();
  url_data_ = nullptr;

  std::string data_str(1, 'a');
  loader->DidReceiveData(data_str);
}

TEST_F(ResourceMultiBufferDataProviderTest,
       RedirectToSameUrlReplacesUrlDataInUrlIndex) {
  Initialize(kHttpUrl, 0);
  Start();
  PartialResponse(0, 100, 1024);

  // Drop the test's reference so `url_index_` holds the sole reference to
  // `UrlData`.
  ResourceMultiBufferDataProvider* loader = loader_.ExtractAsDangling();
  url_data_ = nullptr;

  // Redirect to the same URL with a no-cache response so `url_data_->Valid()`
  // becomes false. In `DidReceiveResponse()`, `GetByUrl()` creates a new
  // `UrlData` for the same key and `TryInsert()` replaces the old `UrlData` in
  // `url_index_`, which must not destroy `url_data_` and `loader` before
  // `loader` is migrated to the new `UrlData`.
  WebURLResponse redirect_response(url_);
  redirect_response.SetHttpHeaderField(WebString::FromUtf8("Cache-Control"),
                                       WebString::FromUtf8("no-cache"));
  EXPECT_CALL(*this, RedirectCallback(_))
      .WillOnce(Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));
  ASSERT_TRUE(loader->WillFollowRedirect(WebURL(url_), redirect_response));

  WebURLResponse response(url_);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 0-100/1024"));
  response.SetExpectedContentLength(101);
  response.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                              WebString::FromUtf8("bytes"));
  response.SetHttpStatusCode(kHttpPartialContent);
  loader->DidReceiveResponse(response);
  EXPECT_TRUE(url_data_);
}

TEST_F(ResourceMultiBufferDataProviderTest, DestructedUrlIndexStartViaReader) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  url_ = KURL(kHttpUrl);
  url_data_ = url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED,
                                   UrlData::kNormal, security_origin_);

  std::unique_ptr<MultiBufferReader> reader;
  url_data_->OnRedirect(blink::BindOnce(
      [](std::unique_ptr<MultiBufferReader>* reader_ptr,
         const scoped_refptr<UrlData>& destination) {
        EXPECT_FALSE(destination);
        reader_ptr->reset();
      },
      Unretained(&reader)));

  url_index_.reset();

  // Creating a reader and calling SetPreload() invokes
  // MultiBufferReader::UpdateInternalState() -> MultiBuffer::AddReader() ->
  // ResourceMultiBuffer::CreateWriter() ->
  // ResourceMultiBufferDataProvider::Start(). When `url_index_` is already
  // destroyed, `Start()` must not synchronously run `url_data_->Fail()` and
  // destroy `reader` while `AddReader()` and `UpdateInternalState()` are still
  // on the stack.
  reader = std::make_unique<MultiBufferReader>(
      url_data_->multibuffer(), 0, 1024 * 1024,
      /*is_client_audio_element=*/false, base::DoNothing(),
      task_environment_.GetMainThreadTaskRunner());
  reader->SetPinRange(0, 1024 * 1024);
  reader->SetPreload(kDataSize, kDataSize);
  ASSERT_TRUE(reader);

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(reader);
}

TEST_F(ResourceMultiBufferDataProviderTest, ServiceWorkerFailureDoesNotRetry) {
  Initialize(kHttpUrl, 100);
  Start();

  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 100-1023/1024"));
  response.SetExpectedContentLength(924);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(std::vector<WebURL>{url_});
  loader_->DidReceiveResponse(response);

  // Failing after receiving a service worker response at `pos_ != 0` should
  // fail immediately rather than scheduling a retry.
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(0);
  loader_->DidFail(WebURLError(net::ERR_ABORTED, url_));
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       PartialResponseMismatchedInstanceSizeFails) {
  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(10);
  Start();

  // First response pins `url_data_->length()` to 1,000,000.
  WebURLResponse response1(url_);
  response1.SetHttpStatusCode(kHttpOK);
  response1.SetExpectedContentLength(1000000);
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 1000000);

  // Truncated non-empty body triggers a retry from byte 1.
  loader_->DidReceiveData(std::string(1, 'a'));
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();

  // A subsequent 206 response reporting a different instance size (7777) must
  // be rejected immediately in DidReceiveResponse().
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response2(url_);
  response2.SetHttpStatusCode(kHttpPartialContent);
  response2.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 1-7776/7777"));
  response2.SetExpectedContentLength(7776);
  loader_->DidReceiveResponse(response2);
  EXPECT_EQ(url_data_->length(), 1000000);
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       OkResponseMismatchedContentLengthFails) {
  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(10);
  Start();

  // First response pins `url_data_->length()` to 1,000,000.
  WebURLResponse response1(url_);
  response1.SetHttpStatusCode(kHttpOK);
  response1.SetExpectedContentLength(1000000);
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 1000000);

  // Truncated non-empty body triggers a retry from byte 1.
  loader_->DidReceiveData(std::string(1, 'a'));
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();

  // A subsequent chunked 200 OK response (kPositionNotSpecified) must be
  // rejected when `url_data_->length()` is already known.
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response2(url_);
  response2.SetHttpStatusCode(kHttpOK);
  response2.SetExpectedContentLength(kPositionNotSpecified);
  loader_->DidReceiveResponse(response2);
  EXPECT_EQ(url_data_->length(), 1000000);
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();

  // Likewise, on a fresh load whose length is pinned to 1,000,000, a
  // mid-resource 200 OK response reporting a different Content-Length (e.g.
  // b/574206996) must be rejected without overwriting `url_data_->length()`.
  Initialize(kHttpUrl, 0);
  reader = CreateReader(10);
  Start();
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 1000000);

  auto mid_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/2, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_mid_loader = mid_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(mid_loader));
  loader_ = raw_mid_loader;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response3(url_);
  response3.SetHttpStatusCode(kHttpOK);
  response3.SetExpectedContentLength(500000);
  loader_->DidReceiveResponse(response3);
  EXPECT_EQ(url_data_->length(), 1000000);
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       EmptyBodyResponseDoesNotRetryInDidFinishLoading) {
  Initialize(kHttpUrl, 0);
  Start();

  // Any 0-byte response whose Content-Length > 0 (whether an ordinary network
  // 200 OK or an opaque HEAD response returned by a service worker) must fail
  // immediately in DidFinishLoading() rather than retrying from byte 0, and
  // must evict the failed UrlData from UrlIndex.
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpOK);
  response.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                              WebString::FromUtf8("bytes"));
  response.SetExpectedContentLength(2345);
  url_data_->Use();
  loader_->DidReceiveResponse(response);
  EXPECT_EQ(url_data_,
            url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(0);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_NE(url_data_,
            url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_));
  testing::Mock::VerifyAndClearExpectations(&fetch_context_);
  StopWhenLoad();

  // Even after block 0 is cached in `url_data_->multibuffer()->map()`, a
  // mid-resource range request that receives 0 payload bytes (or only bytes
  // discarded by `bytes_to_discard_`) must fail immediately rather than
  // retrying.
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  constexpr int kBlockSize = 1 << 15;
  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();

  WebURLResponse initial_206(url_);
  initial_206.SetHttpStatusCode(kHttpPartialContent);
  initial_206.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/100000"));
  initial_206.SetExpectedContentLength(kBlockSize);
  loader_->DidReceiveResponse(initial_206);
  loader_->DidReceiveData(std::string(kBlockSize, 'a'));
  ASSERT_FALSE(url_data_->multibuffer()->map().empty());

  auto mid_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/2, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_mid_loader = mid_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(mid_loader));
  loader_ = raw_mid_loader;
  Start();

  WebURLResponse empty_mid_206(url_);
  empty_mid_206.SetHttpStatusCode(kHttpPartialContent);
  empty_mid_206.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 65532-99999/100000"));
  empty_mid_206.SetExpectedContentLength(100000 - 65532);
  loader_->DidReceiveResponse(empty_mid_206);
  // Deliver 4 pre-range bytes that are completely discarded before EOF.
  loader_->DidReceiveData(std::string(4, 'x'));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(0);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       ServiceWorkerShortResponseDoesNotRetryInDidFinishLoading) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  constexpr int kBlockSize = 1 << 15;

  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();

  // An opaque service worker response whose decoded body is shorter than its
  // Content-Length (even when `range_supported()` is true via Accept-Ranges)
  // must fail immediately in DidFinishLoading() rather than issuing a retry
  // whose Range header would disclose the decoded byte count.
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpOK);
  response.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                              WebString::FromUtf8("bytes"));
  response.SetExpectedContentLength(5000);
  response.SetType(network::mojom::FetchResponseType::kOpaque);
  response.SetWasFetchedViaServiceWorker(true);
  response.SetUrlListViaServiceWorker(std::vector<WebURL>{url_});
  loader_->DidReceiveResponse(response);
  EXPECT_TRUE(url_data_->range_supported());
  EXPECT_TRUE(url_data_->is_cors_cross_origin());

  loader_->DidReceiveData(std::string(1337, 'x'));
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(0);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       ServiceWorkerPartialResponseSubBlockRetriesInDidFinishLoading) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  constexpr int kBlockSize = 1 << 15;

  Initialize(kHttpUrl, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();

  // A non-opaque 206 Partial Content response from a service worker that
  // delivers fewer bytes than a 32 KiB MultiBuffer block (leaving `fifo_`
  // non-empty and `multibuffer()->map()` empty) must still retry from the next
  // byte offset in DidFinishLoading().
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 0-98/1000"));
  response.SetExpectedContentLength(99);
  response.SetWasFetchedViaServiceWorker(true);
  loader_->DidReceiveResponse(response);

  loader_->DidReceiveData(std::string(99, 'A'));
  EXPECT_TRUE(url_data_->multibuffer()->map().empty());
  EXPECT_CALL(*this, RedirectCallback(_)).Times(0);
  EXPECT_CALL(fetch_context_, CreateUrlLoader(_)).Times(1);
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();
  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       OpaqueServiceWorkerRejectsDifferentUrlAndMidResourceOkResponse) {
  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  constexpr int kBlockSize = 1 << 15;
  constexpr char kRequestUrl[] = "https://attacker.example/audio.mp3";
  constexpr char kSecretUrl[] = "https://victim.example/secret";
  constexpr char kProbeUrl[] = "https://victim.example/probe?len=100000";

  Initialize(kRequestUrl, 0);
  auto reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  url_data_->Use();
  Start();

  // 1. Initial opaque service worker 206 response pins `kSecretUrl` and length
  // 100,000, and must not be inserted into `url_index_`.
  WebURL secret_url{KURL(kSecretUrl)};
  WebURLResponse response1{secret_url};
  response1.SetHttpStatusCode(kHttpPartialContent);
  response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-32767/100000"));
  response1.SetExpectedContentLength(kBlockSize);
  response1.SetType(network::mojom::FetchResponseType::kOpaque);
  response1.SetWasFetchedViaServiceWorker(true);
  response1.SetUrlListViaServiceWorker(std::vector<WebURL>{secret_url});
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 100000);
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  EXPECT_NE(url_data_,
            url_index_->GetByUrl(KURL(kRequestUrl), UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_));

  loader_->DidReceiveData(std::string(kBlockSize, 'a'));

  // 2. A follow-up range request at block 1 answered by the service worker with
  // a different same-origin URL (`kProbeUrl`) must be rejected even if its
  // instance size matches 100,000 (preventing b/576134162 size probing).
  auto range_loader1 = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_range_loader1 = range_loader1.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(range_loader1));
  loader_ = raw_range_loader1;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURL probe_url{KURL(kProbeUrl)};
  WebURLResponse probe_response{probe_url};
  probe_response.SetHttpStatusCode(kHttpPartialContent);
  probe_response.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/100000"));
  probe_response.SetExpectedContentLength(kBlockSize);
  probe_response.SetType(network::mojom::FetchResponseType::kOpaque);
  probe_response.SetWasFetchedViaServiceWorker(true);
  probe_response.SetUrlListViaServiceWorker(std::vector<WebURL>{probe_url});
  loader_->DidReceiveResponse(probe_response);

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();

  // 3. On a fresh load, a mid-resource (pos > 0) 200 OK opaque service worker
  // response (e.g. from a tampered RequestInit that stripped Range) must be
  // rejected even for the exact same URL, whereas a 206 Partial Content
  // pass-through response for the exact same URL succeeds.
  Initialize(kRequestUrl, 0);
  reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();
  loader_->DidReceiveResponse(response1);
  loader_->DidReceiveData(std::string(kBlockSize, 'a'));

  auto range_loader2 = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_range_loader2 = range_loader2.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(range_loader2));
  loader_ = raw_range_loader2;
  Start();

  WebURLResponse same_url_206{secret_url};
  same_url_206.SetHttpStatusCode(kHttpPartialContent);
  same_url_206.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/100000"));
  same_url_206.SetExpectedContentLength(kBlockSize);
  same_url_206.SetType(network::mojom::FetchResponseType::kOpaque);
  same_url_206.SetWasFetchedViaServiceWorker(true);
  same_url_206.SetUrlListViaServiceWorker(std::vector<WebURL>{secret_url});
  EXPECT_CALL(*this, RedirectCallback(_)).Times(0);
  loader_->DidReceiveResponse(same_url_206);

  auto range_loader3 = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/2, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_range_loader3 = range_loader3.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(range_loader3));
  loader_ = raw_range_loader3;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse same_url_200{secret_url};
  same_url_200.SetHttpStatusCode(kHttpOK);
  same_url_200.SetExpectedContentLength(100000);
  same_url_200.SetType(network::mojom::FetchResponseType::kOpaque);
  same_url_200.SetWasFetchedViaServiceWorker(true);
  same_url_200.SetUrlListViaServiceWorker(std::vector<WebURL>{secret_url});
  loader_->DidReceiveResponse(same_url_200);

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();

  // 4. Mixing an opaque service worker response with a native opaque response
  // (or vice versa) on the same UrlData is rejected even when both return 206
  // for the exact same URL.
  Initialize(kSecretUrl, 0);
  reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();
  loader_->DidReceiveResponse(response1);
  loader_->DidReceiveData(std::string(kBlockSize, 'a'));

  auto sw_to_native_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_sw_to_native = sw_to_native_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(sw_to_native_loader));
  loader_ = raw_sw_to_native;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse native_206{secret_url};
  native_206.SetHttpStatusCode(kHttpPartialContent);
  native_206.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/100000"));
  native_206.SetExpectedContentLength(kBlockSize);
  native_206.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(native_206);

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();

  Initialize(kSecretUrl, 0);
  reader = CreateReader(kBlockSize);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(loader_);
  Start();
  WebURLResponse native_initial_206{secret_url};
  native_initial_206.SetHttpStatusCode(kHttpPartialContent);
  native_initial_206.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 0-32767/100000"));
  native_initial_206.SetExpectedContentLength(kBlockSize);
  native_initial_206.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(native_initial_206);
  loader_->DidReceiveData(std::string(kBlockSize, 'a'));

  auto native_to_sw_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_native_to_sw = native_to_sw_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(native_to_sw_loader));
  loader_ = raw_native_to_sw;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  loader_->DidReceiveResponse(same_url_206);

  loader_ = nullptr;
  reader.reset();
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest,
       FailedRedirectDoesNotMutateOrInvalidateCachedDestinationUrlData) {
  // 1. Seed `url_index_` with a healthy cached entry for `kHttpRedirect`
  // (using `ServiceWorkerMode::kSkip`, which `WillFollowRedirect()` selects for
  // network redirects when `kDisallowMixedOpaqueMediaResponses` is enabled).
  KURL dest_url(kHttpRedirect);
  scoped_refptr<UrlData> cached_dest = url_index_->GetByUrl(
      dest_url, UrlData::CORS_UNSPECIFIED, UrlData::kNormal, security_origin_,
      media::DataSource::EncodingMode::kIdentity,
      UrlData::ServiceWorkerMode::kSkip);
  cached_dest->Use();
  cached_dest->set_range_supported();
  cached_dest->set_length(1024);
  cached_dest->set_etag(kEtag);
  cached_dest->set_cacheable(true);
  cached_dest->SetDataOrigin(dest_url, /*is_cors_cross_origin=*/false);
  ASSERT_TRUE(cached_dest->Valid());
  ASSERT_EQ(cached_dest, url_index_->TryInsert(cached_dest));

  bool cached_dest_redirect_called = false;
  cached_dest->OnRedirect(blink::BindOnce(
      [](bool* called, const scoped_refptr<UrlData>&) { *called = true; },
      Unretained(&cached_dest_redirect_called)));

  // 2. Start a request to `kHttpUrl` that redirects to `kHttpRedirect` and
  // fails with HTTP 404 carrying different headers.
  Initialize(kHttpUrl, 0);
  Start();
  WebURL redirect_target{dest_url};
  WebURLResponse redirect_response(url_);
  ASSERT_TRUE(loader_->WillFollowRedirect(redirect_target, redirect_response));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse not_found_response(dest_url);
  not_found_response.SetHttpStatusCode(404);
  not_found_response.SetHttpHeaderField(WebString::FromUtf8("ETag"),
                                        WebString::FromUtf8("\"clobbered\""));
  not_found_response.SetHttpHeaderField(WebString::FromUtf8("Cache-Control"),
                                        WebString::FromUtf8("no-store"));
  loader_->DidReceiveResponse(not_found_response);

  // `cached_dest` must remain valid, unmodified, and indexed in `url_index_`.
  EXPECT_FALSE(cached_dest_redirect_called);
  EXPECT_TRUE(cached_dest->Valid());
  EXPECT_EQ(cached_dest->length(), 1024);
  EXPECT_EQ(cached_dest->etag(), kEtag);
  EXPECT_TRUE(cached_dest->cacheable());
  EXPECT_EQ(cached_dest,
            url_index_->GetByUrl(dest_url, UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_,
                                 media::DataSource::EncodingMode::kIdentity,
                                 UrlData::ServiceWorkerMode::kSkip));
  StopWhenLoad();

  // 3. Repeat with a redirected 206 response whose instance size mismatches
  // `cached_dest->length()`.
  Initialize(kHttpUrl, 0);
  Start();
  ASSERT_TRUE(loader_->WillFollowRedirect(redirect_target, redirect_response));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse mismatched_206(dest_url);
  mismatched_206.SetHttpStatusCode(kHttpPartialContent);
  mismatched_206.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                    WebString::FromUtf8("bytes 0-99/9999"));
  mismatched_206.SetExpectedContentLength(100);
  mismatched_206.SetHttpHeaderField(WebString::FromUtf8("ETag"),
                                    WebString::FromUtf8("\"clobbered\""));
  loader_->DidReceiveResponse(mismatched_206);

  EXPECT_FALSE(cached_dest_redirect_called);
  EXPECT_TRUE(cached_dest->Valid());
  EXPECT_EQ(cached_dest->length(), 1024);
  EXPECT_EQ(cached_dest->etag(), kEtag);
  EXPECT_EQ(cached_dest,
            url_index_->GetByUrl(dest_url, UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_,
                                 media::DataSource::EncodingMode::kIdentity,
                                 UrlData::ServiceWorkerMode::kSkip));
  StopWhenLoad();

  // 4. Verify that a cross-origin redirect that fails with HTTP 404 still marks
  // the failing initial UrlData as CORS cross-origin.
  Initialize("http://localhost:18080/start", 0);
  Start();
  scoped_refptr<UrlData> cross_origin_failed_data = url_data_;
  WebURL cross_origin_target{KURL("http://127.0.0.1:18081/missing")};
  WebURLResponse cross_redirect_response(url_);
  ASSERT_TRUE(loader_->WillFollowRedirect(cross_origin_target,
                                          cross_redirect_response));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse cross_404(cross_origin_target);
  cross_404.SetHttpStatusCode(404);
  loader_->DidReceiveResponse(cross_404);
  EXPECT_TRUE(cross_origin_failed_data->is_cors_cross_origin());
  StopWhenLoad();

  // 5. Verify that redirecting from a UrlData with a known length to a cached
  // non-HTTP UrlData whose response reports unknown `ExpectedContentLength()`
  // (`kPositionNotSpecified`) fails cleanly without mutating the cached entry
  // or crashing in `RedirectTo()` when the existing lengths conflict.
  security_origin_ = SecurityOrigin::Create(KURL("file:///"));
  KURL file_dest_url("file:///tmp/dest.webm");
  scoped_refptr<UrlData> cached_file_dest = url_index_->GetByUrl(
      file_dest_url, UrlData::CORS_UNSPECIFIED, UrlData::kNormal,
      security_origin_, media::DataSource::EncodingMode::kIdentity,
      UrlData::ServiceWorkerMode::kSkip);
  cached_file_dest->Use();
  cached_file_dest->set_range_supported();
  cached_file_dest->set_length(1024);
  cached_file_dest->SetDataOrigin(file_dest_url,
                                  /*is_cors_cross_origin=*/false);
  ASSERT_TRUE(cached_file_dest->Valid());
  ASSERT_EQ(cached_file_dest, url_index_->TryInsert(cached_file_dest));

  Initialize("file:///tmp/src.webm", 0);
  Start();
  url_data_->SetDataOrigin(url_, /*is_cors_cross_origin=*/false);
  url_data_->set_length(4096);
  WebURL file_redirect_target{file_dest_url};
  WebURLResponse file_redirect_response(url_);
  ASSERT_TRUE(loader_->WillFollowRedirect(file_redirect_target,
                                          file_redirect_response));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse unknown_len_file_response(file_dest_url);
  unknown_len_file_response.SetExpectedContentLength(kPositionNotSpecified);
  loader_->DidReceiveResponse(unknown_len_file_response);

  EXPECT_TRUE(cached_file_dest->Valid());
  EXPECT_EQ(cached_file_dest->length(), 1024);
  StopWhenLoad();
}

// Regression test for crbug.com/579225207: if an earlier request on `UrlData`
// fails pre-response via `CrossOriginFail()` (`is_cors_cross_origin() == true`
// while `data_origin_` is unset), a subsequent non-opaque response must be
// rejected cleanly in `DidReceiveResponse()` rather than tripping the
// post-`TryInsert()` / post-`RedirectTo()` `CHECK`.
TEST_F(ResourceMultiBufferDataProviderTest,
       NonOpaqueResponseAfterCrossOriginFailFailsCleanly) {
  base::test::ScopedFeatureList feature_list(
      media::kDisallowMixedOpaqueMediaResponses);

  // Seed `url_index_` with a healthy cached entry for `kHttpRedirect` so we
  // also verify an orphaned in-flight redirect provider on a failed `UrlData`
  // cannot hijack or invalidate a shared `UrlIndex` entry.
  KURL dest_url(kHttpRedirect);
  scoped_refptr<UrlData> cached_dest = url_index_->GetByUrl(
      dest_url, UrlData::CORS_UNSPECIFIED, UrlData::kNormal, security_origin_,
      media::DataSource::EncodingMode::kIdentity,
      UrlData::ServiceWorkerMode::kSkip);
  cached_dest->Use();
  cached_dest->set_range_supported();
  cached_dest->set_length(1024);
  cached_dest->SetDataOrigin(dest_url, /*is_cors_cross_origin=*/false);
  ASSERT_TRUE(cached_dest->Valid());
  ASSERT_EQ(cached_dest, url_index_->TryInsert(cached_dest));

  Initialize(kHttpUrl, 0);
  ResourceMultiBufferDataProvider* first_loader = loader_;
  Start();

  // Start two concurrent in-flight range providers (`pos = 1` direct and
  // `pos = 2` redirected to `kHttpRedirect`) before `first_loader` fails via
  // `CrossOriginFail()`.
  auto second_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_second_loader = second_loader.get();
  url_data_->multibuffer()->AddProvider(std::move(second_loader));
  loader_ = raw_second_loader;
  Start();

  auto redir_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/2, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_redir_loader = redir_loader.get();
  url_data_->multibuffer()->AddProvider(std::move(redir_loader));
  loader_ = raw_redir_loader;
  Start();
  WebURL redir_target{dest_url};
  WebURLResponse redir_response(url_);
  redir_response.SetHttpStatusCode(302);
  ASSERT_TRUE(
      raw_redir_loader->WillFollowRedirect(redir_target, redir_response));

  // `first_loader` fails with `ERR_BLOCKED_BY_ORB`, marking `url_data_` as
  // failed and CORS cross-origin.
  loader_ = first_loader;
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  first_loader->DidFail(WebURLError(net::ERR_BLOCKED_BY_ORB, url_));
  EXPECT_TRUE(url_data_->is_cors_cross_origin());
  EXPECT_TRUE(url_data_->HasFailed());

  // In-flight `raw_second_loader` receiving a non-opaque response must fail
  // cleanly.
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 32768-33791/65536"));
  response.SetExpectedContentLength(1024);
  response.SetType(network::mojom::FetchResponseType::kBasic);
  raw_second_loader->DidReceiveResponse(response);

  // In-flight `raw_redir_loader` completing its redirect to `kHttpRedirect`
  // must also fail cleanly without mutating or invalidating `cached_dest`.
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse target_response(redir_target);
  target_response.SetHttpStatusCode(kHttpPartialContent);
  target_response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                     WebString::FromUtf8("bytes 0-1023/1024"));
  target_response.SetExpectedContentLength(1024);
  target_response.SetType(network::mojom::FetchResponseType::kBasic);
  raw_redir_loader->DidReceiveResponse(target_response);

  EXPECT_TRUE(cached_dest->Valid());
  EXPECT_FALSE(cached_dest->is_cors_cross_origin());
  EXPECT_EQ(cached_dest,
            url_index_->GetByUrl(dest_url, UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_,
                                 media::DataSource::EncodingMode::kIdentity,
                                 UrlData::ServiceWorkerMode::kSkip));
  StopWhenLoad();
}

// Regression test for crbug.com/579225207: an opaque-origin `response_url`
// (such as a `blob:null/...` URL inside a sandboxed iframe after
// `URL.revokeObjectURL()` removes its `BlobURLNullOriginMap` entry while
// `BackgroundURLLoader` is in flight) must not fail `CanAcceptDataFrom()`
// against itself on the post-`TryInsert()` `CHECK`.
TEST_F(ResourceMultiBufferDataProviderTest,
       RevokedNullOriginBlobResponseDoesNotCrash) {
  base::test::ScopedFeatureList disable_mixed_opaque;
  disable_mixed_opaque.InitAndDisableFeature(
      media::kDisallowMixedOpaqueMediaResponses);

  constexpr char kRevokedNullBlobUrl[] =
      "blob:null/12345678-1234-1234-1234-123456789abc";
  Initialize(kRevokedNullBlobUrl, 0);
  Start();

  WebURLResponse response{WebURL(KURL(kRevokedNullBlobUrl))};
  response.SetHttpStatusCode(kHttpOK);
  response.SetExpectedContentLength(kDataSize);
  response.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response);
  EXPECT_EQ(url_data_->length(), kDataSize);
  StopWhenLoad();
}

// Verifies that after an initial no-cors cross-origin bounce (A1 -> B -> A2),
// subsequent range requests on the same `UrlData` use their own per-request
// redirect state rather than reading the sticky `has_cross_origin_redirect()`
// flag from Request 1. A follow-up range request that skips the cross-origin
// hop (either via a purely same-origin `A1 -> A2` `kBasic` redirect or via an
// unredirected `kOpaque` response for `A2`) must be rejected by
// `CanAcceptDataFrom()`.
TEST_F(ResourceMultiBufferDataProviderTest,
       SubsequentNonCrossOriginRedirectAfterCrossOriginBounceFails) {
  constexpr char kOriginA1[] = "https://victim.example/start";
  constexpr char kOriginB[] = "https://attacker.example/bounce";
  constexpr char kOriginA2[] = "https://victim.example/target";
  constexpr int kBlockSize = 1 << 15;

  // Case 1: Follow-up range request performs a same-origin `A1 -> A2` redirect
  // (`kBasic`) without any cross-origin hop.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    security_origin_ = SecurityOrigin::Create(KURL(kOriginA1));

    Initialize(kOriginA1, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURL url_b{KURL(kOriginB)};
    WebURL url_a2{KURL(kOriginA2)};
    WebURLResponse redir_to_b{KURL(kOriginA1)};
    redir_to_b.SetHttpStatusCode(302);
    WebURLResponse redir_to_a2{url_b};
    redir_to_a2.SetHttpStatusCode(302);

    ASSERT_TRUE(loader_->WillFollowRedirect(url_b, redir_to_b));
    ASSERT_TRUE(loader_->WillFollowRedirect(url_a2, redir_to_a2));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlDataAndObserveRedirects(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{url_a2};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/65536"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response1);
    ASSERT_TRUE(loader_);
    EXPECT_TRUE(url_data_->is_cors_cross_origin());
    EXPECT_TRUE(url_data_->has_cross_origin_redirect());

    std::string data_str(kBlockSize, 'a');
    loader_->DidReceiveData(data_str);

    // Follow-up range request redirects same-origin `A1 -> A2` without `B`.
    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);

    WebURLResponse same_origin_redir{KURL(kOriginA1)};
    same_origin_redir.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(url_a2, same_origin_redir));

    EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
    WebURLResponse response2{url_a2};
    response2.SetHttpStatusCode(kHttpPartialContent);
    response2.SetHttpHeaderField(
        WebString::FromUtf8("Content-Range"),
        WebString::FromUtf8("bytes 32768-65535/65536"));
    response2.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response2.SetExpectedContentLength(kBlockSize);
    response2.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response2);

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }

  // Case 2: Follow-up range request receives an unredirected `kOpaque` response
  // with `ResponseUrl() == A2` (matching both `data_origin()` and
  // `is_cors_cross_origin()`, differing only in per-request redirect state).
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    security_origin_ = SecurityOrigin::Create(KURL(kOriginA1));

    Initialize(kOriginA1, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURL url_b{KURL(kOriginB)};
    WebURL url_a2{KURL(kOriginA2)};
    WebURLResponse redir_to_b{KURL(kOriginA1)};
    redir_to_b.SetHttpStatusCode(302);
    WebURLResponse redir_to_a2{url_b};
    redir_to_a2.SetHttpStatusCode(302);

    ASSERT_TRUE(loader_->WillFollowRedirect(url_b, redir_to_b));
    ASSERT_TRUE(loader_->WillFollowRedirect(url_a2, redir_to_a2));

    EXPECT_CALL(*this, RedirectCallback(_))
        .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
          reader.reset();
          SetUrlDataAndObserveRedirects(new_url_data);
          reader = CreateReader(kBlockSize * 2);
        });
    WebURLResponse response1{url_a2};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/65536"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response1);
    ASSERT_TRUE(loader_);

    std::string data_str(kBlockSize, 'a');
    loader_->DidReceiveData(data_str);

    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);

    EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
    WebURLResponse response2{url_a2};
    response2.SetHttpStatusCode(kHttpPartialContent);
    response2.SetHttpHeaderField(
        WebString::FromUtf8("Content-Range"),
        WebString::FromUtf8("bytes 32768-65535/65536"));
    response2.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response2.SetExpectedContentLength(kBlockSize);
    response2.SetType(network::mojom::FetchResponseType::kOpaque);
    response2.SetWasFetchedViaServiceWorker(true);
    response2.SetUrlListViaServiceWorker(std::vector<WebURL>{url_a2});
    loader_->DidReceiveResponse(response2);

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }
}

// Regression test for b/581380020: a mid-resource HTTP 416 response (whether
// direct or after a redirect to a fresh UrlData) must be rejected when
// `byte_pos() < url_data_->length()`, and a valid HTTP 416 at
// `byte_pos() >= url_data_->length()` after a redirect must propagate the
// pinned `length()` onto the destination UrlData so a subsequent response
// cannot overwrite it with a different resource size.
TEST_F(ResourceMultiBufferDataProviderTest,
       RangeNotSatisfiableValidatesAndPreservesPinnedLength) {
  constexpr char kUrl1[] = "https://example.com/part1.mp4";
  constexpr char kUrl2[] = "https://example.com/part2.mp4";
  constexpr int kBlockSize = 1 << 15;
  constexpr int64_t kPinnedLength = 65536;

  // Case 1: Mid-resource (`byte_pos() < length()`) HTTP 416 after a redirect
  // to a fresh UrlData is rejected without unpinning `length()`.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    security_origin_ = SecurityOrigin::Create(KURL(kUrl1));

    Initialize(kUrl1, 0);
    auto reader = CreateReader(kBlockSize * 2);
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);
    loader_->Start();

    WebURL url1{KURL(kUrl1)};
    WebURL url2{KURL(kUrl2)};
    WebURLResponse response1{url1};
    response1.SetHttpStatusCode(kHttpPartialContent);
    response1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                 WebString::FromUtf8("bytes 0-32767/65536"));
    response1.SetHttpHeaderField(WebString::FromUtf8("Accept-Ranges"),
                                 WebString::FromUtf8("bytes"));
    response1.SetExpectedContentLength(kBlockSize);
    response1.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response1);
    ASSERT_TRUE(loader_);
    EXPECT_EQ(url_data_->length(), kPinnedLength);

    loader_->DidReceiveData(std::string(kBlockSize, 'a'));
    loader_->DidFinishLoading();
    task_environment_.FastForwardUntilNoTasksRemain();
    ASSERT_TRUE(loader_);

    // Follow-up range request at block 1 (`byte_pos() == 32768 < 65536`)
    // redirects same-origin to `kUrl2` and returns HTTP 416.
    WebURLResponse redir_to_2{url1};
    redir_to_2.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(url2, redir_to_2));

    EXPECT_CALL(*this, RedirectCallback(testing::IsNull()));
    WebURLResponse response_416{url2};
    response_416.SetHttpStatusCode(416);
    response_416.SetType(network::mojom::FetchResponseType::kBasic);
    loader_->DidReceiveResponse(response_416);
    EXPECT_EQ(url_data_->length(), kPinnedLength);

    loader_ = nullptr;
    reader.reset();
    StopWhenLoad();
  }

  // Case 2: Valid HTTP 416 at `byte_pos() >= length()` after a redirect to a
  // fresh UrlData propagates `kPinnedLength` onto the redirected UrlData.
  {
    url_index_ = std::make_unique<UrlIndex>(
        &fetch_context_, task_environment_.GetMainThreadTaskRunner());
    security_origin_ = SecurityOrigin::Create(KURL(kUrl1));

    // Start an in-flight range request at block 2 (`byte_pos() == 65536`)
    // before `length_` is known, then pin `length_` to `65536` (as if an
    // earlier provider on the same `UrlData` completed its header parse while
    // this provider was in flight).
    Initialize(kUrl1, 2);
    Start();
    url_data_->SetDataOrigin(KURL(kUrl1), /*is_cors_cross_origin=*/false);
    url_data_->set_length(kPinnedLength);

    WebURL url1{KURL(kUrl1)};
    WebURL url2{KURL(kUrl2)};
    WebURLResponse redir_to_2{url1};
    redir_to_2.SetHttpStatusCode(302);
    ASSERT_TRUE(loader_->WillFollowRedirect(url2, redir_to_2));

    EXPECT_CALL(*this, RedirectCallback(testing::NotNull()))
        .WillOnce(
            Invoke(this, &ResourceMultiBufferDataProviderTest::SetUrlData));
    WebURLResponse response_416{url2};
    response_416.SetHttpStatusCode(416);
    response_416.SetType(network::mojom::FetchResponseType::kBasic);
    loader_.ExtractAsDangling()->DidReceiveResponse(response_416);

    ASSERT_TRUE(url_data_);
    EXPECT_EQ(url_data_->url(), KURL(kUrl2));
    EXPECT_EQ(url_data_->length(), kPinnedLength);
    StopWhenLoad();
  }
}

// Verifies that when a Service Worker intercepts a local media request (e.g.
// `/local.mp4`) with `Response.redirect('https://cdn.com/video.mp4')` and still
// provides the target response via the Service Worker (which `CorsURLLoader`
// marks `kOpaque` due to the cross-origin redirect hop), the load uses an
// unindexed `UrlData` that replays `/local.mp4` on follow-up range requests and
// neither fails against nor mutates an existing native `UrlIndex` entry for
// `https://cdn.com/video.mp4`.
TEST_F(ResourceMultiBufferDataProviderTest,
       ServiceWorkerOpaqueRedirectUsesUnindexedUrlDataWithoutColliding) {
  constexpr char kPageOrigin[] = "https://example.com";
  constexpr char kLocalUrl[] = "https://example.com/local.mp4";
  constexpr char kCdnUrl[] = "https://cdn.com/video.mp4";
  constexpr int kBlockSize = 1 << 15;

  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  security_origin_ = SecurityOrigin::Create(KURL(kPageOrigin));

  // 1. First load `https://cdn.com/video.mp4` natively (`length = 50000`),
  // populating `url_index_` under `(kCdnUrl, kAllow)`.
  Initialize(kCdnUrl, 0);
  auto cdn_reader = CreateReader(kBlockSize);
  ASSERT_TRUE(loader_);
  url_data_->Use();
  Start();

  WebURL cdn_web_url{KURL(kCdnUrl)};
  WebURLResponse native_cdn_206{cdn_web_url};
  native_cdn_206.SetHttpStatusCode(kHttpPartialContent);
  native_cdn_206.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                    WebString::FromUtf8("bytes 0-32767/50000"));
  native_cdn_206.SetExpectedContentLength(kBlockSize);
  native_cdn_206.SetType(network::mojom::FetchResponseType::kOpaque);
  loader_->DidReceiveResponse(native_cdn_206);
  loader_->DidReceiveData(std::string(kBlockSize, 'c'));

  scoped_refptr<UrlData> indexed_cdn_data = url_data_;
  ASSERT_EQ(indexed_cdn_data,
            url_index_->GetByUrl(KURL(kCdnUrl), UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_));
  EXPECT_EQ(indexed_cdn_data->length(), 50000);
  EXPECT_FALSE(indexed_cdn_data->HasOpaqueServiceWorkerResponse());
  loader_ = nullptr;
  cdn_reader.reset();
  StopWhenLoad();

  // 2. Now load `/local.mp4`, where the Service Worker responds with
  // `Response.redirect('https://cdn.com/video.mp4')` and then serves local
  // 99,999-byte media chunks through the Service Worker (tainted `kOpaque` by
  // `CorsURLLoader`).
  Initialize(kLocalUrl, 0);
  auto local_reader = CreateReader(kBlockSize * 2);
  ASSERT_TRUE(loader_);
  url_data_->Use();
  loader_->Start();

  WebURL local_web_url{KURL(kLocalUrl)};
  WebURLResponse sw_redirect{local_web_url};
  sw_redirect.SetHttpStatusCode(302);
  sw_redirect.SetWasFetchedViaServiceWorker(true);
  ASSERT_TRUE(loader_->WillFollowRedirect(cdn_web_url, sw_redirect));

  EXPECT_CALL(*this, RedirectCallback(testing::NotNull()))
      .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
        local_reader.reset();
        SetUrlDataAndObserveRedirects(new_url_data);
        local_reader = CreateReader(kBlockSize * 2);
      });
  WebURLResponse sw_opaque_local_bytes{cdn_web_url};
  sw_opaque_local_bytes.SetHttpStatusCode(kHttpPartialContent);
  sw_opaque_local_bytes.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 0-32767/99999"));
  sw_opaque_local_bytes.SetExpectedContentLength(kBlockSize);
  sw_opaque_local_bytes.SetType(network::mojom::FetchResponseType::kOpaque);
  sw_opaque_local_bytes.SetWasFetchedViaServiceWorker(true);
  sw_opaque_local_bytes.SetUrlListViaServiceWorker(
      std::vector<WebURL>{local_web_url});
  loader_->DidReceiveResponse(sw_opaque_local_bytes);
  ASSERT_TRUE(url_data_);
  EXPECT_NE(url_data_, indexed_cdn_data);
  EXPECT_EQ(url_data_->url(), KURL(kCdnUrl));
  EXPECT_EQ(url_data_->request_url(), KURL(kLocalUrl));
  EXPECT_TRUE(url_data_->has_original_request_url());
  EXPECT_TRUE(url_data_->HasOpaqueServiceWorkerResponse());
  EXPECT_EQ(url_data_->length(), 99999);

  // `indexed_cdn_data` in `url_index_` must remain valid and untouched.
  EXPECT_TRUE(indexed_cdn_data->Valid());
  EXPECT_EQ(indexed_cdn_data->length(), 50000);
  EXPECT_FALSE(indexed_cdn_data->HasOpaqueServiceWorkerResponse());
  EXPECT_EQ(indexed_cdn_data,
            url_index_->GetByUrl(KURL(kCdnUrl), UrlData::CORS_UNSPECIFIED,
                                 UrlData::kNormal, security_origin_));

  // 3. Follow-up range request at block 1 replays `/local.mp4` and succeeds.
  loader_->DidReceiveData(std::string(kBlockSize, 'l'));
  auto range_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_range_loader = range_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(range_loader));
  loader_ = raw_range_loader;
  Start();
  ASSERT_TRUE(loader_->WillFollowRedirect(cdn_web_url, sw_redirect));

  EXPECT_CALL(*this, RedirectCallback(_)).Times(0);
  WebURLResponse sw_opaque_local_bytes_part2{cdn_web_url};
  sw_opaque_local_bytes_part2.SetHttpStatusCode(kHttpPartialContent);
  sw_opaque_local_bytes_part2.SetHttpHeaderField(
      WebString::FromUtf8("Content-Range"),
      WebString::FromUtf8("bytes 32768-65535/99999"));
  sw_opaque_local_bytes_part2.SetExpectedContentLength(kBlockSize);
  sw_opaque_local_bytes_part2.SetType(
      network::mojom::FetchResponseType::kOpaque);
  sw_opaque_local_bytes_part2.SetWasFetchedViaServiceWorker(true);
  sw_opaque_local_bytes_part2.SetUrlListViaServiceWorker(
      std::vector<WebURL>{local_web_url});
  loader_->DidReceiveResponse(sw_opaque_local_bytes_part2);

  loader_ = nullptr;
  local_reader.reset();
  StopWhenLoad();
}

// Verifies that Service Worker redirect + opaque response sequences do not trip
// the post-`TryInsert()` or post-`RedirectTo()` `CHECK` even when
// `kDisallowMixedOpaqueMediaResponses` is disabled.
TEST_F(ResourceMultiBufferDataProviderTest,
       ServiceWorkerRedirectWithKillSwitchDisabledDoesNotCrash) {
  base::test::ScopedFeatureList disable_mixed_opaque;
  disable_mixed_opaque.InitAndDisableFeature(
      media::kDisallowMixedOpaqueMediaResponses);

  constexpr char kPageOrigin[] = "https://example.com";
  constexpr char kUrlA[] = "https://cdn.com/a.mp4";
  constexpr char kUrlB[] = "https://cdn.com/b.mp4";
  constexpr char kUrlC[] = "https://cdn.com/c.mp4";
  constexpr int kBlockSize = 1 << 15;

  url_index_ = std::make_unique<UrlIndex>(
      &fetch_context_, task_environment_.GetMainThreadTaskRunner());
  security_origin_ = SecurityOrigin::Create(KURL(kPageOrigin));

  // V1 loads `cdn.com/a.mp4`: block 0 responds with opaque SW fetch of
  // `cdn.com/b.mp4`, and follow-up range request responds with opaque SW fetch
  // of `cdn.com/a.mp4` (rejected cleanly due to URL mismatch).
  Initialize(kUrlA, 0);
  auto reader1 = CreateReader(kBlockSize * 2);
  ASSERT_TRUE(loader_);
  url_data_->Use();
  Start();

  WebURL url_a{KURL(kUrlA)};
  WebURL url_b{KURL(kUrlB)};
  WebURL url_c{KURL(kUrlC)};
  WebURLResponse v1_resp0{url_b};
  v1_resp0.SetHttpStatusCode(kHttpPartialContent);
  v1_resp0.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 0-32767/65536"));
  v1_resp0.SetExpectedContentLength(kBlockSize);
  v1_resp0.SetType(network::mojom::FetchResponseType::kOpaque);
  v1_resp0.SetWasFetchedViaServiceWorker(true);
  v1_resp0.SetUrlListViaServiceWorker(std::vector<WebURL>{url_b});
  loader_->DidReceiveResponse(v1_resp0);
  loader_->DidReceiveData(std::string(kBlockSize, 'b'));

  auto v1_range_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_v1_range_loader = v1_range_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(v1_range_loader));
  loader_ = raw_v1_range_loader;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse v1_resp1{url_a};
  v1_resp1.SetHttpStatusCode(kHttpPartialContent);
  v1_resp1.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 32768-65535/65536"));
  v1_resp1.SetExpectedContentLength(kBlockSize);
  v1_resp1.SetType(network::mojom::FetchResponseType::kOpaque);
  v1_resp1.SetWasFetchedViaServiceWorker(true);
  v1_resp1.SetUrlListViaServiceWorker(std::vector<WebURL>{url_a});
  loader_->DidReceiveResponse(v1_resp1);
  loader_ = nullptr;
  reader1.reset();
  StopWhenLoad();

  // V2 loads `cdn.com/c.mp4` and redirects via `Response.redirect` to
  // `cdn.com/a.mp4` with an opaque SW response.
  Initialize(kUrlC, 0);
  auto reader2 = CreateReader(kBlockSize);
  ASSERT_TRUE(loader_);
  Start();

  WebURLResponse sw_redir{url_c};
  sw_redir.SetHttpStatusCode(302);
  sw_redir.SetWasFetchedViaServiceWorker(true);
  ASSERT_TRUE(loader_->WillFollowRedirect(url_a, sw_redir));

  EXPECT_CALL(*this, RedirectCallback(testing::NotNull()))
      .WillOnce([&](const scoped_refptr<UrlData>& new_url_data) {
        reader2.reset();
        SetUrlDataAndObserveRedirects(new_url_data);
        reader2 = CreateReader(kBlockSize);
      });
  WebURLResponse v2_resp{url_a};
  v2_resp.SetHttpStatusCode(kHttpPartialContent);
  v2_resp.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                             WebString::FromUtf8("bytes 0-32767/65536"));
  v2_resp.SetExpectedContentLength(kBlockSize);
  v2_resp.SetType(network::mojom::FetchResponseType::kOpaque);
  v2_resp.SetWasFetchedViaServiceWorker(true);
  v2_resp.SetUrlListViaServiceWorker(std::vector<WebURL>{url_a});
  loader_->DidReceiveResponse(v2_resp);
  ASSERT_TRUE(url_data_);
  EXPECT_TRUE(url_data_->has_original_request_url());
  EXPECT_EQ(url_data_->request_url(), KURL(kUrlC));

  loader_ = nullptr;
  reader2.reset();
  StopWhenLoad();
}

}  // namespace blink
