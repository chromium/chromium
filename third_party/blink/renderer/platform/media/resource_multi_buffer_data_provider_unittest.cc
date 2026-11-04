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
  Start();
  PartialResponse(0, 2048, 32000);
  Redirect(kHttpRedirect);
  PartialResponse(2048, 4096, 32000);
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
  Start();

  // First response pins `url_data_->length()` to 1,000,000.
  WebURLResponse response1(url_);
  response1.SetHttpStatusCode(kHttpOK);
  response1.SetExpectedContentLength(1000000);
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 1000000);

  // Empty body triggers a retry from byte 0.
  loader_->DidFinishLoading();
  task_environment_.FastForwardUntilNoTasksRemain();

  // A subsequent 206 response reporting a different instance size (7777) must
  // be rejected immediately in DidReceiveResponse().
  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response2(url_);
  response2.SetHttpStatusCode(kHttpPartialContent);
  response2.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                               WebString::FromUtf8("bytes 0-7776/7777"));
  response2.SetExpectedContentLength(7777);
  loader_->DidReceiveResponse(response2);
  EXPECT_EQ(url_data_->length(), 1000000);
}

TEST_F(ResourceMultiBufferDataProviderTest,
       OkResponseMismatchedContentLengthFails) {
  Initialize(kHttpUrl, 0);
  Start();

  // First response pins `url_data_->length()` to 1,000,000.
  WebURLResponse response1(url_);
  response1.SetHttpStatusCode(kHttpOK);
  response1.SetExpectedContentLength(1000000);
  loader_->DidReceiveResponse(response1);
  EXPECT_EQ(url_data_->length(), 1000000);

  // Empty body triggers a retry from byte 0.
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

  // Likewise, a mid-resource 200 OK response reporting a different
  // Content-Length (e.g. b/574206996) must be rejected without overwriting
  // `url_data_->length()`.
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  auto mid_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/1, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_mid_loader = mid_loader.get();
  url_data_->multibuffer()->AddProvider(std::move(mid_loader));
  loader_ = raw_mid_loader;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response3(url_);
  response3.SetHttpStatusCode(kHttpOK);
  response3.SetExpectedContentLength(500000);
  loader_->DidReceiveResponse(response3);
  EXPECT_EQ(url_data_->length(), 1000000);
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

  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  loader_->DidFail(WebURLError(net::ERR_BLOCKED_BY_ORB, url_));
  EXPECT_TRUE(url_data_->is_cors_cross_origin());

  // Start a second provider on the same `url_data_` (which still has no
  // `data_origin_` set) and deliver a non-opaque response.
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  auto second_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/0, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_second_loader = second_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(second_loader));
  loader_ = raw_second_loader;
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse response(url_);
  response.SetHttpStatusCode(kHttpPartialContent);
  response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                              WebString::FromUtf8("bytes 0-1023/1024"));
  response.SetExpectedContentLength(1024);
  response.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(response);

  // Also verify the redirect path when `old_url_data` had `CrossOriginFail()`
  // and an in-flight request redirects to a fresh destination `UrlData`.
  url_data_->OnRedirect(
      blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                      Unretained(this)));
  auto redir_loader = std::make_unique<ResourceMultiBufferDataProvider>(
      url_data_.get(), /*pos=*/0, /*is_client_audio_element=*/false,
      task_environment_.GetMainThreadTaskRunner());
  ResourceMultiBufferDataProvider* raw_redir_loader = redir_loader.get();
  loader_ = nullptr;
  url_data_->multibuffer()->AddProvider(std::move(redir_loader));
  loader_ = raw_redir_loader;
  Start();

  WebURL redir_target{KURL(kHttpRedirect)};
  WebURLResponse redir_response(url_);
  redir_response.SetHttpStatusCode(302);
  ASSERT_TRUE(loader_->WillFollowRedirect(redir_target, redir_response));

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));
  WebURLResponse target_response(redir_target);
  target_response.SetHttpStatusCode(kHttpPartialContent);
  target_response.SetHttpHeaderField(WebString::FromUtf8("Content-Range"),
                                     WebString::FromUtf8("bytes 0-1023/1024"));
  target_response.SetExpectedContentLength(1024);
  target_response.SetType(network::mojom::FetchResponseType::kBasic);
  loader_->DidReceiveResponse(target_response);
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

}  // namespace blink
