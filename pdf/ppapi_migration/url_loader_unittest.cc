// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "url/gurl.h"

namespace chrome_pdf {
namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SaveArg;
using ::testing::UnorderedElementsAreArray;

constexpr char kOriginUrl[] = "http://example.com/";
constexpr char kDocumentUrl[] = "http://example.com/embedder/index.html";
constexpr base::span<const char> kFakeData = "fake data";

size_t GetRequestHeaderCount(const blink::WebURLRequest& request) {
  struct : public blink::WebHTTPHeaderVisitor {
    void VisitHeader(const blink::WebString& name,
                     const blink::WebString& value) override {
      ++count;
    }

    size_t count = 0;
  } counting_header_visitor;

  request.VisitHttpHeaderFields(&counting_header_visitor);
  return counting_header_visitor.count;
}

blink::WebURLError MakeWebURLError(int reason) {
  return blink::WebURLError(reason, GURL());
}

class MockWebAssociatedURLLoader : public blink::WebAssociatedURLLoader {
 public:
  // blink::WebAssociatedURLLoader:
  MOCK_METHOD(void,
              LoadAsynchronously,
              (const blink::WebURLRequest&,
               blink::WebAssociatedURLLoaderClient*),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void, SetDefersLoading, (bool), (override));
  MOCK_METHOD(void,
              SetLoadingTaskRunner,
              (base::SingleThreadTaskRunner*),
              (override));
};

class FakeBlinkUrlLoaderClient : public BlinkUrlLoader::Client {
 public:
  base::WeakPtr<FakeBlinkUrlLoaderClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

  void Invalidate() { valid_ = false; }

  MockWebAssociatedURLLoader* mock_url_loader() {
    return mock_url_loader_.get();
  }

  const blink::WebAssociatedURLLoaderOptions& saved_options() const {
    return saved_options_;
  }

  // BlinkUrlLoader::Client:
  bool IsValid() const override { return valid_; }

  blink::WebURL CompleteURL(
      const blink::WebString& partial_url) const override {
    EXPECT_TRUE(IsValid());
    return GURL(kDocumentUrl).Resolve(partial_url.Utf8());
  }

  net::SiteForCookies SiteForCookies() const override {
    EXPECT_TRUE(IsValid());
    return net::SiteForCookies::FromUrl(GURL(kOriginUrl));
  }

  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override {
    EXPECT_FALSE(referrer_url.IsEmpty());
    EXPECT_TRUE(IsValid());
    request.SetReferrerString(referrer_url.GetString());
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override {
    EXPECT_TRUE(IsValid());
    EXPECT_TRUE(mock_url_loader_);
    saved_options_ = options;
    return std::move(mock_url_loader_);
  }

 private:
  bool valid_ = true;

  std::unique_ptr<NiceMock<MockWebAssociatedURLLoader>> mock_url_loader_ =
      std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
  blink::WebAssociatedURLLoaderOptions saved_options_;

  base::WeakPtrFactory<FakeBlinkUrlLoaderClient> weak_factory_{this};
};

class BlinkUrlLoaderTest : public testing::Test {
 protected:
  BlinkUrlLoaderTest() {
    ON_CALL(*mock_url_loader_, LoadAsynchronously)
        .WillByDefault(
            Invoke(this, &BlinkUrlLoaderTest::FakeLoadAsynchronously));
    loader_ = std::make_unique<BlinkUrlLoader>(fake_client_.GetWeakPtr());
  }

  void FakeLoadAsynchronously(const blink::WebURLRequest& request,
                              blink::WebAssociatedURLLoaderClient* client) {
    saved_request_.CopyFrom(request);
    EXPECT_EQ(loader_.get(), client);
  }

  void StartLoadWithThresholds(size_t lower, size_t upper) {
    UrlRequest request;
    request.buffer_lower_threshold = lower;
    request.buffer_upper_threshold = upper;
    loader_->Open(request, mock_callback_.Get());
    loader_->DidReceiveResponse(blink::WebURLResponse());
  }

  int32_t DidFailWithError(const blink::WebURLError& error) {
    int32_t result = 0;
    loader_->Open(UrlRequest(), mock_callback_.Get());
    EXPECT_CALL(mock_callback_, Run).WillOnce(SaveArg<0>(&result));

    loader_->DidFail(error);
    return result;
  }

  FakeBlinkUrlLoaderClient fake_client_;
  NiceMock<base::MockCallback<ResultCallback>> mock_callback_;
  std::unique_ptr<BlinkUrlLoader> loader_;

  // Becomes invalid if `loader_` is closed or destructed.
  MockWebAssociatedURLLoader* mock_url_loader_ = fake_client_.mock_url_loader();

  blink::WebURLRequest saved_request_;
};

TEST_F(BlinkUrlLoaderTest, GrantUniversalAccess) {
  loader_->GrantUniversalAccess();
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_TRUE(fake_client_.saved_options().grant_universal_access);
}

TEST_F(BlinkUrlLoaderTest, Open) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously);
  EXPECT_CALL(mock_callback_, Run).Times(0);

  UrlRequest request;
  request.url = "http://example.com/fake.pdf";
  request.method = "FAKE";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_FALSE(fake_client_.saved_options().grant_universal_access);
  EXPECT_EQ(GURL("http://example.com/fake.pdf"), GURL(saved_request_.Url()));
  EXPECT_EQ("FAKE", saved_request_.HttpMethod().Ascii());
  EXPECT_EQ(GURL(kOriginUrl),
            saved_request_.SiteForCookies().RepresentativeUrl());
  EXPECT_TRUE(saved_request_.GetSkipServiceWorker());
  EXPECT_EQ(0u, GetRequestHeaderCount(saved_request_));
  EXPECT_EQ(blink::mojom::RequestContextType::PLUGIN,
            saved_request_.GetRequestContext());
  EXPECT_EQ(network::mojom::RequestDestination::kEmbed,
            saved_request_.GetRequestDestination());
}

TEST_F(BlinkUrlLoaderTest, OpenWithInvalidatedClientWeakPtr) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously).Times(0);
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  fake_client_.InvalidateWeakPtrs();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, OpenWithInvalidatedClient) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously).Times(0);
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  fake_client_.Invalidate();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, OpenWithRelativeUrl) {
  UrlRequest request;
  request.url = "relative.pdf";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_EQ(GURL(kDocumentUrl).Resolve("relative.pdf"),
            GURL(saved_request_.Url()));
}

TEST_F(BlinkUrlLoaderTest, OpenWithHeaders) {
  UrlRequest request;
  request.headers = base::JoinString(
      {
          "Content-Length: 123",
          "Content-Type: application/pdf",
          "Non-ASCII-Value: 🙃",
      },
      "\n");
  loader_->Open(request, mock_callback_.Get());

  EXPECT_EQ(3u, GetRequestHeaderCount(saved_request_));
  EXPECT_EQ("123", saved_request_.HttpHeaderField("Content-Length").Utf8());
  EXPECT_EQ("application/pdf",
            saved_request_.HttpHeaderField("Content-Type").Utf8());
  EXPECT_EQ("🙃", saved_request_.HttpHeaderField("Non-ASCII-Value").Utf8());
}

TEST_F(BlinkUrlLoaderTest, OpenWithBody) {
  UrlRequest request;
  request.body = "fake body";
  loader_->Open(request, mock_callback_.Get());

  blink::WebHTTPBody request_body = saved_request_.HttpBody();
  EXPECT_EQ(1u, request_body.ElementCount());

  blink::WebHTTPBody::Element element;
  EXPECT_TRUE(request_body.ElementAt(0, element));
  EXPECT_EQ(blink::WebHTTPBody::Element::kTypeData, element.type);

  std::string data;
  element.data.ForEachSegment(
      [&](const char* segment, size_t length, size_t pos) {
        data.append(segment, length);
        return true;
      });
  EXPECT_EQ("fake body", data);
}

TEST_F(BlinkUrlLoaderTest, OpenWithCustomReferrerUrl) {
  UrlRequest request;
  request.custom_referrer_url = "http://example.com/referrer";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_EQ("http://example.com/referrer",
            saved_request_.ReferrerString().Utf8());
}

TEST_F(BlinkUrlLoaderTest, WillFollowRedirect) {
  loader_->Open(UrlRequest(), mock_callback_.Get());

  EXPECT_TRUE(loader_->WillFollowRedirect(GURL("http://example.com/login"),
                                          blink::WebURLResponse()));
}

TEST_F(BlinkUrlLoaderTest, WillFollowRedirectWhileIgnoringRedirects) {
  UrlRequest request;
  request.ignore_redirects = true;
  loader_->Open(request, mock_callback_.Get());

  EXPECT_FALSE(loader_->WillFollowRedirect(GURL("http://example.com/login"),
                                           blink::WebURLResponse()));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveResponse) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_OK));

  blink::WebURLResponse response;
  response.SetHttpStatusCode(204);
  loader_->DidReceiveResponse(response);

  EXPECT_EQ(204, loader_->response().status_code);
  EXPECT_EQ("", loader_->response().headers);
}

TEST_F(BlinkUrlLoaderTest, DidReceiveResponseWithHeaders) {
  loader_->Open(UrlRequest(), mock_callback_.Get());

  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Length", "123");
  response.AddHttpHeaderField("Content-Type", "application/pdf");
  response.AddHttpHeaderField("Non-ASCII-Value", "🙃");
  loader_->DidReceiveResponse(response);

  std::vector<std::string> split_headers =
      base::SplitString(loader_->response().headers, "\n",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  EXPECT_THAT(split_headers, UnorderedElementsAreArray({
                                 "Content-Length: 123",
                                 "Content-Type: application/pdf",
                                 "Non-ASCII-Value: 🙃",
                             }));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveData) {
  char buffer[kFakeData.size()] = {};
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveDataWithZeroLength) {
  char buffer[kFakeData.size()] = {};
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->DidReceiveData(kFakeData.data(), 0);

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveDataBelowUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  char buffer[3] = {};
  loader_->DidReceiveData(buffer, sizeof(buffer));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveDataCrossUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char read_buffer[1];
  loader_->ReadResponseBody(read_buffer, mock_callback_.Get());
  {
    InSequence defer_before_read_callback;
    EXPECT_CALL(*mock_url_loader_, SetDefersLoading(true));
    EXPECT_CALL(mock_callback_, Run);
  }

  char buffer[4] = {};
  loader_->DidReceiveData(buffer, sizeof(buffer));
}

TEST_F(BlinkUrlLoaderTest, DidReceiveDataAboveUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char buffer[4] = {};
  loader_->DidReceiveData(buffer, sizeof(buffer));
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  loader_->DidReceiveData(buffer, sizeof(buffer));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBody) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData));

  // Verify no more data returned on next call.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWithoutData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWithEmptyBuffer) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_BADARGUMENT));

  loader_->ReadResponseBody(base::span<char>(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWithSmallerBuffer) {
  static constexpr size_t kTailSize = 1;
  static constexpr size_t kBufferSize = kFakeData.size() - kTailSize;

  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());
  EXPECT_CALL(mock_callback_, Run(kBufferSize));

  char buffer[kBufferSize] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData.first(kBufferSize)));

  // Verify remaining data returned on next call.
  char tail_buffer[kTailSize];
  EXPECT_CALL(mock_callback_, Run(kTailSize));
  loader_->ReadResponseBody(tail_buffer, mock_callback_.Get());
  EXPECT_THAT(tail_buffer, ElementsAreArray(kFakeData.subspan(kBufferSize)));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWithBiggerBuffer) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  char buffer[kFakeData.size() + 1] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  base::span<char> buffer_span = buffer;
  EXPECT_THAT(buffer_span.first(kFakeData.size()), ElementsAreArray(kFakeData));
  EXPECT_THAT(buffer_span.subspan(kFakeData.size()), Each(0));

  // Verify no more data returned on next call.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWhileLoadComplete) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());
  loader_->DidFinishLoading();
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData));

  // Verify no more data returned on next call.
  char tail_buffer[kFakeData.size()] = {};
  EXPECT_CALL(mock_callback_, Run(0));
  loader_->ReadResponseBody(tail_buffer, mock_callback_.Get());
  EXPECT_THAT(tail_buffer, Each(0));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWhileLoadCompleteWithoutData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidFinishLoading();
  EXPECT_CALL(mock_callback_, Run(0));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWhileLoadCompleteWithError) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData.data(), kFakeData.size());
  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyAboveLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer, sizeof(write_buffer));
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  char buffer[2] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyCrossLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer, sizeof(write_buffer));
  {
    InSequence resume_before_read_callback;
    EXPECT_CALL(*mock_url_loader_, SetDefersLoading(false));
    EXPECT_CALL(mock_callback_, Run);
  }

  char buffer[3] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyBelowLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer, sizeof(write_buffer));

  char buffer[3] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, DidFinishLoading) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->DidFinishLoading();
}

TEST_F(BlinkUrlLoaderTest, DidFinishLoadingWithPendingCallback) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(0));  // Result represents read bytes.

  loader_->DidFinishLoading();
}

TEST_F(BlinkUrlLoaderTest, DidFailWhileOpening) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
}

TEST_F(BlinkUrlLoaderTest, DidFailWhileStreamingData) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
}

TEST_F(BlinkUrlLoaderTest, DidFailWithErrorAccessDenied) {
  int32_t result = DidFailWithError(MakeWebURLError(net::ERR_ACCESS_DENIED));

  EXPECT_EQ(PP_ERROR_NOACCESS, result);
}

TEST_F(BlinkUrlLoaderTest, DidFailWithErrorNetworkAccessDenied) {
  int32_t result =
      DidFailWithError(MakeWebURLError(net::ERR_NETWORK_ACCESS_DENIED));

  EXPECT_EQ(PP_ERROR_NOACCESS, result);
}

TEST_F(BlinkUrlLoaderTest, DidFailWithWebSecurityViolationError) {
  blink::WebURLError error(network::CorsErrorStatus(),
                           blink::WebURLError::HasCopyInCache::kFalse, GURL());
  ASSERT_TRUE(error.is_web_security_violation());

  int32_t result = DidFailWithError(error);

  EXPECT_EQ(PP_ERROR_NOACCESS, result);
}

TEST_F(BlinkUrlLoaderTest, CloseWhileWaitingToOpen) {
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseWhileOpening) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_ABORTED));

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseWhileStreamingData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseWhileStreamingDataWithPendingCallback) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_ABORTED));

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseWhileLoadComplete) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidFinishLoading();
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseAgain) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->Close();
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

}  // namespace
}  // namespace chrome_pdf
