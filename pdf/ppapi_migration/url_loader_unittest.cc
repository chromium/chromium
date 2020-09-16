// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "net/base/net_errors.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
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

using ::testing::_;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::ReturnNull;
using ::testing::SaveArg;

constexpr base::span<const char> kFakeData = "fake data";

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

class MockBlinkUrlLoaderClient : public BlinkUrlLoader::Client {
 public:
  base::WeakPtr<MockBlinkUrlLoaderClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

  // BlinkUrlLoader::Client:
  MOCK_METHOD(std::unique_ptr<blink::WebAssociatedURLLoader>,
              CreateAssociatedURLLoader,
              (const blink::WebAssociatedURLLoaderOptions&),
              (override));

 private:
  base::WeakPtrFactory<MockBlinkUrlLoaderClient> weak_factory_{this};
};

class BlinkUrlLoaderTest : public testing::Test {
 protected:
  BlinkUrlLoaderTest() {
    ON_CALL(mock_client_, CreateAssociatedURLLoader(_))
        .WillByDefault(
            Invoke(this, &BlinkUrlLoaderTest::FakeCreateAssociatedURLLoader));
    ON_CALL(*mock_url_loader_, LoadAsynchronously(_, _))
        .WillByDefault(
            Invoke(this, &BlinkUrlLoaderTest::FakeLoadAsynchronously));
    loader_ = std::make_unique<BlinkUrlLoader>(mock_client_.GetWeakPtr());
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> FakeCreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) {
    EXPECT_TRUE(mock_url_loader_);
    saved_options_ = options;
    return std::move(mock_url_loader_);
  }

  void FakeLoadAsynchronously(const blink::WebURLRequest& request,
                              blink::WebAssociatedURLLoaderClient* client) {
    saved_request_.CopyFrom(request);
    EXPECT_EQ(loader_.get(), client);
  }

  int32_t DidFailWithError(const blink::WebURLError& error) {
    int32_t result = 0;
    loader_->Open(UrlRequest(), mock_callback_.Get());
    EXPECT_CALL(mock_callback_, Run(_)).WillOnce(SaveArg<0>(&result));

    loader_->DidFail(error);
    return result;
  }

  NiceMock<MockBlinkUrlLoaderClient> mock_client_;
  NiceMock<base::MockCallback<ResultCallback>> mock_callback_;
  std::unique_ptr<BlinkUrlLoader> loader_;

  std::unique_ptr<NiceMock<MockWebAssociatedURLLoader>> mock_url_loader_ =
      std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
  blink::WebAssociatedURLLoaderOptions saved_options_;
  blink::WebURLRequest saved_request_;
};

TEST_F(BlinkUrlLoaderTest, GrantUniversalAccess) {
  loader_->GrantUniversalAccess();
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_TRUE(saved_options_.grant_universal_access);
}

TEST_F(BlinkUrlLoaderTest, Open) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_));
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously(_, _));
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

  UrlRequest request;
  request.url = "http://example.com/fake.pdf";
  request.method = "FAKE";
  loader_->Open(request, mock_callback_.Get());

  EXPECT_FALSE(saved_options_.grant_universal_access);
  EXPECT_EQ(GURL("http://example.com/fake.pdf"), GURL(saved_request_.Url()));
  EXPECT_EQ("FAKE", saved_request_.HttpMethod().Ascii());
  EXPECT_EQ(blink::mojom::RequestContextType::PLUGIN,
            saved_request_.GetRequestContext());
  EXPECT_EQ(network::mojom::RequestDestination::kEmbed,
            saved_request_.GetRequestDestination());
}

TEST_F(BlinkUrlLoaderTest, OpenWithInvalidatedClient) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_)).Times(0);
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously(_, _)).Times(0);
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  mock_client_.InvalidateWeakPtrs();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, OpenWithFailingCreateAssociatedURLLoader) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_))
      .WillOnce(ReturnNull());
  EXPECT_CALL(*mock_url_loader_, LoadAsynchronously(_, _)).Times(0);
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, DidReceiveResponse) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(PP_OK));

  blink::WebURLResponse response;
  response.SetHttpStatusCode(204);
  loader_->DidReceiveResponse(response);

  EXPECT_EQ(204, loader_->response().status_code);
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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

  loader_->DidReceiveData(kFakeData.data(), 0);

  EXPECT_THAT(buffer, Each(0));
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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);
  loader_->ReadResponseBody(buffer, mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, ReadResponseBodyWithoutData) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);
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

TEST_F(BlinkUrlLoaderTest, DidFinishLoading) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->DidReceiveResponse(blink::WebURLResponse());
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

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
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

  loader_->Close();
}

TEST_F(BlinkUrlLoaderTest, CloseAgain) {
  loader_->Open(UrlRequest(), mock_callback_.Get());
  loader_->Close();
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

  loader_->Close();
}

}  // namespace
}  // namespace chrome_pdf
