// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/url_loader.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/loader/result_codes.h"
#include "pdf/test/mock_web_associated_url_loader.h"
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
constexpr base::span<const char> kFakeData =
    base::span_from_cstring("fake data");

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

class FakeUrlLoaderClient : public UrlLoader::Client {
 public:
  base::WeakPtr<FakeUrlLoaderClient> GetWeakPtr() {
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

  // UrlLoader::Client:
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

  base::WeakPtrFactory<FakeUrlLoaderClient> weak_factory_{this};
};

class UrlLoaderTest : public testing::Test {
 protected:
  UrlLoaderTest() {
    ON_CALL(*mock_url_loader_, LoadAsynchronously)
        .WillByDefault(Invoke(this, &UrlLoaderTest::FakeLoadAsynchronously));
    loader_ = std::make_unique<UrlLoader>(fake_client_.GetWeakPtr());
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

  FakeUrlLoaderClient fake_client_;
  NiceMock<base::MockCallback<base::OnceCallback<void(int)>>> mock_callback_;
  std::unique_ptr<UrlLoader> loader_;

  // Becomes invalid if `loader_` is closed or destructed.
  raw_ptr<MockWebAssociatedURLLoader, DisableDanglingPtrDetection>
      mock_url_loader_ = fake_client_.mock_url_loader();

  blink::WebURLRequest saved_request_;
};

TEST_F(UrlLoaderTest, Open) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously);
  EXPECT_CALL(mock_callback_, Run).Times(0);

  UrlRequest request;
  request.url = "http://example.com/fake.pdf";
  request.method = "FAKE";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_TRUE(fake_client_.saved_options().grant_universal_access);
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

TEST_F(UrlLoaderTest, OpenWithInvalidatedClientWeakPtr) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously).Times(0);
  EXPECT_CALL(mock_callback_, Run(Result::kErrorFailed));

  fake_client_.InvalidateWeakPtrs();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(UrlLoaderTest, OpenWithInvalidatedClient) {
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously).Times(0);
  EXPECT_CALL(mock_callback_, Run(Result::kErrorFailed));

  fake_client_.Invalidate();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(UrlLoaderTest, OpenWithRelativeUrl) {
  UrlRequest request;
  request.url = "relative.pdf";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_EQ(GURL(kDocumentUrl).Resolve("relative.pdf"),
            GURL(saved_request_.Url()));
}

TEST_F(UrlLoaderTest, OpenWithHeaders) {
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

TEST_F(UrlLoaderTest, OpenWithBody) {
  UrlRequest request;
  request.body = "fake body";
  loader_->Open(request, mock_callback_.Get());

  blink::WebHTTPBody request_body = saved_request_.HttpBody();
  EXPECT_EQ(1u, request_body.ElementCount());

  blink::WebHTTPBody::Element element;
  EXPECT_TRUE(request_body.ElementAt(0, element));
  EXPECT_EQ(blink::HTTPBodyElementType::kTypeData, element.type);

  std::string data;
  element.data.ForEachSegment(
      [&](const char* segment, size_t length, size_t pos) {
        data.append(segment, length);
        return true;
      });
  EXPECT_EQ("fake body", data);
}

TEST_F(UrlLoaderTest, OpenWithCustomReferrerUrl) {
  UrlRequest request;
  request.custom_referrer_url = "http://example.com/referrer";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_EQ("http://example.com/referrer",
            saved_request_.ReferrerString().Utf8());
}

TEST_F(UrlLoaderTest, WillFollowRedirect) {
  loader_->Open(UrlRequest(), mock_callback_.Get());

  EXPECT_TRUE(loader_->WillFollowRedirect(GURL("http://example.com/login"),
                                          blink::WebURLResponse()));
}

TEST_F(UrlLoaderTest, WillFollowRedirectWhileIgnoringRedirects) {
  UrlRequest request;
  request.ignore_redirects = true;
  loader_->Open(request, mock_callback_.Get());

  EXPECT_FALSE(loader_->WillFollowRedirect(GURL("http://example.com/login"),
                                           blink::WebURLResponse()));
}

TEST_F(UrlLoaderTest, DidReceiveResponse) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(Result::kSuccess));

  blink::WebURLResponse response;
  response.SetHttpStatusCode(204);
  loader_->DidReceiveResponse(response);

  EXPECT_EQ(204, loader_->response().status_code);
  EXPECT_EQ("", loader_->response().headers);
}

TEST_F(UrlLoaderTest, DidReceiveResponseWithHeaders) {
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

TEST_F(UrlLoaderTest, DidReceiveData) {
  char buffer[kFakeData.size()] = {};
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  loader_->DidReceiveData(kFakeData);

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData));
}

TEST_F(UrlLoaderTest, DidReceiveDataWithZeroLength) {
  char buffer[kFakeData.size()] = {};
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->DidReceiveData(kFakeData.first(0u));

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(UrlLoaderTest, DidReceiveDataBelowUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  char buffer[3] = {};
  loader_->DidReceiveData(buffer);
}

TEST_F(UrlLoaderTest, DidReceiveDataCrossUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char read_buffer[1];
  loader_->ReadResponseBody(read_buffer, mock_callback_.Get());
  {
    InSequence defer_before_read_callback;
    EXPECT_CALL(*mock_url_loader_, SetDefersLoading(true));
    EXPECT_CALL(mock_callback_, Run);
  }

  char buffer[4] = {};
  loader_->DidReceiveData(buffer);
}

TEST_F(UrlLoaderTest, DidReceiveDataAboveUpperThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char buffer[4] = {};
  loader_->DidReceiveData(buffer);
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  loader_->DidReceiveData(buffer);
}

TEST_F(UrlLoaderTest, ReadResponseBody) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData);
  EXPECT_CALL(mock_callback_, Run(kFakeData.size()));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, ElementsAreArray(kFakeData));

  // Verify no more data returned on next call.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(UrlLoaderTest, ReadResponseBodyWithoutData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(UrlLoaderTest, ReadResponseBodyWithEmptyBuffer) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run(Result::kErrorBadArgument));

  loader_->ReadResponseBody(base::span<char>(), mock_callback_.Get());
}

TEST_F(UrlLoaderTest, ReadResponseBodyWithSmallerBuffer) {
  static constexpr size_t kTailSize = 1;
  static constexpr size_t kBufferSize = kFakeData.size() - kTailSize;

  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData);
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

TEST_F(UrlLoaderTest, ReadResponseBodyWithBiggerBuffer) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData);
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

TEST_F(UrlLoaderTest, ReadResponseBodyWhileLoadComplete) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData);
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

TEST_F(UrlLoaderTest, ReadResponseBodyWhileLoadCompleteWithoutData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidFinishLoading();
  EXPECT_CALL(mock_callback_, Run(0));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(UrlLoaderTest, ReadResponseBodyWhileLoadCompleteWithError) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidReceiveData(kFakeData);
  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
  EXPECT_CALL(mock_callback_, Run(Result::kErrorFailed));

  char buffer[kFakeData.size()] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());

  EXPECT_THAT(buffer, Each(0));
}

TEST_F(UrlLoaderTest, ReadResponseBodyAboveLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer);
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  char buffer[2] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(UrlLoaderTest, ReadResponseBodyCrossLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer);
  {
    InSequence resume_before_read_callback;
    EXPECT_CALL(*mock_url_loader_, SetDefersLoading(false));
    EXPECT_CALL(mock_callback_, Run);
  }

  char buffer[3] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(UrlLoaderTest, ReadResponseBodyBelowLowerThreshold) {
  StartLoadWithThresholds(/*lower=*/2, /*upper=*/4);

  char write_buffer[5] = {};
  loader_->DidReceiveData(write_buffer);

  char buffer[3] = {};
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(*mock_url_loader_, SetDefersLoading).Times(0);

  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(UrlLoaderTest, DidFinishLoading) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->DidFinishLoading();
}

TEST_F(UrlLoaderTest, DidFinishLoadingWithPendingCallback) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(0));  // Result represents read bytes.

  loader_->DidFinishLoading();
}

TEST_F(UrlLoaderTest, DidFailWhileOpening) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(Result::kErrorFailed));

  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
}

TEST_F(UrlLoaderTest, DidFailWhileStreamingData) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(Result::kErrorFailed));

  loader_->DidFail(MakeWebURLError(net::ERR_FAILED));
}

TEST_F(UrlLoaderTest, DidFailWithErrorAccessDenied) {
  int32_t result = DidFailWithError(MakeWebURLError(net::ERR_ACCESS_DENIED));

  EXPECT_EQ(Result::kErrorNoAccess, result);
}

TEST_F(UrlLoaderTest, DidFailWithErrorNetworkAccessDenied) {
  int32_t result =
      DidFailWithError(MakeWebURLError(net::ERR_NETWORK_ACCESS_DENIED));

  EXPECT_EQ(Result::kErrorNoAccess, result);
}

TEST_F(UrlLoaderTest, DidFailWithWebSecurityViolationError) {
  blink::WebURLError error(
      network::CorsErrorStatus(network::mojom::CorsError::kDisallowedByMode),
      blink::WebURLError::HasCopyInCache::kFalse, GURL());
  ASSERT_TRUE(error.is_web_security_violation());

  int32_t result = DidFailWithError(error);

  EXPECT_EQ(Result::kErrorNoAccess, result);
}

TEST_F(UrlLoaderTest, CloseWhileWaitingToOpen) {
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(UrlLoaderTest, CloseWhileOpening) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(Result::kErrorAborted));

  loader_->Close();
}

TEST_F(UrlLoaderTest, CloseWhileStreamingData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(UrlLoaderTest, CloseWhileStreamingDataWithPendingCallback) {
  char buffer[1];
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(Result::kErrorAborted));

  loader_->Close();
}

TEST_F(UrlLoaderTest, CloseWhileLoadComplete) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  loader_->DidFinishLoading();
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

TEST_F(UrlLoaderTest, CloseAgain) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->Close();
  EXPECT_CALL(mock_callback_, Run).Times(0);

  loader_->Close();
}

}  // namespace
}  // namespace chrome_pdf
