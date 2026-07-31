// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/url_loader_wrapper_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "net/cookies/site_for_cookies.h"
#include "pdf/loader/result_codes.h"
#include "pdf/loader/url_loader.h"
#include "pdf/test/mock_web_associated_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "url/gurl.h"

namespace chrome_pdf {

using ::testing::NiceMock;

constexpr char kOriginUrl[] = "http://example.com/";
constexpr char kDocumentUrl[] = "http://example.com/embedder/index.html";

class FakeUrlLoaderClient : public UrlLoader::Client {
 public:
  base::WeakPtr<FakeUrlLoaderClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // UrlLoader::Client:
  bool IsValid() const override { return true; }

  blink::WebURL CompleteURL(
      const blink::WebString& partial_url) const override {
    return GURL(kDocumentUrl).Resolve(partial_url.Utf8());
  }

  net::SiteForCookies SiteForCookies() const override {
    return net::SiteForCookies::FromUrl(GURL(kOriginUrl));
  }

  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override {
    request.SetReferrerString(referrer_url.GetString());
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override {
    return std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
  }

 private:
  base::WeakPtrFactory<FakeUrlLoaderClient> weak_factory_{this};
};

class URLLoaderWrapperImplTest : public testing::Test {
 protected:
  URLLoaderWrapperImplTest() = default;
  ~URLLoaderWrapperImplTest() override = default;

  std::unique_ptr<URLLoaderWrapperImpl> CreateWrapperWithResponse(
      const blink::WebURLResponse& response) {
    auto url_loader = std::make_unique<UrlLoader>(fake_client_.GetWeakPtr());
    url_loader->Open(UrlRequest(), mock_open_callback_.Get());
    url_loader->DidReceiveResponse(response);
    return std::make_unique<URLLoaderWrapperImpl>(std::move(url_loader));
  }

  FakeUrlLoaderClient fake_client_;
  NiceMock<base::MockCallback<UrlLoader::OpenCallback>> mock_open_callback_;
};

TEST_F(URLLoaderWrapperImplTest, GetByteRangeStartValidContentRange) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-499/1234");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_TRUE(wrapper->GetByteRangeStart(&start));
  EXPECT_EQ(0, start);
}

TEST_F(URLLoaderWrapperImplTest,
       GetByteRangeStartValidContentRangeNonZeroStart) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 100-499/1234");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_TRUE(wrapper->GetByteRangeStart(&start));
  EXPECT_EQ(100, start);
}

TEST_F(URLLoaderWrapperImplTest, GetByteRangeStartValidContentRangeMaxUint32) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-4294967295/4294967296");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_TRUE(wrapper->GetByteRangeStart(&start));
  EXPECT_EQ(0, start);
}

TEST_F(URLLoaderWrapperImplTest,
       GetByteRangeStartInvalidContentRangeNoSlashLength) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-499");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest,
       GetByteRangeStartInvalidContentRangeUnknownLength) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-499/*");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest,
       GetByteRangeStartInvalidContentRangeStartAfterEnd) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 500-499/1000");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest, GetByteRangeStartInvalidContentRangeNonDigit) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-49a/1000");
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest, GetByteRangeStartInvalidContentRangeOverflow) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range",
                              "bytes 4294967296-4294967297/4294967298");
  auto wrapper = CreateWrapperWithResponse(response);

  // TODO(crbug.com/540801224): Support byte ranges > 4 GiB.
  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest,
       GetByteRangeStartInvalidContentRangeEndOverflow) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Range", "bytes 0-4294967296/4294967298");
  auto wrapper = CreateWrapperWithResponse(response);

  // TODO(crbug.com/540801224): Support byte ranges > 4 GiB.
  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest, GetByteRangeStartNoContentRange) {
  blink::WebURLResponse response;
  auto wrapper = CreateWrapperWithResponse(response);

  int start = -1;
  EXPECT_FALSE(wrapper->GetByteRangeStart(&start));
}

TEST_F(URLLoaderWrapperImplTest, ParseHeadersBasic) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Length", "123");
  response.AddHttpHeaderField("Accept-Ranges", "bytes");
  response.AddHttpHeaderField("Content-Encoding", "gzip");
  response.AddHttpHeaderField("Content-Type", "application/pdf");
  response.AddHttpHeaderField("Content-Disposition",
                              "inline; filename=test.pdf");

  auto wrapper = CreateWrapperWithResponse(response);
  EXPECT_EQ(123, wrapper->GetContentLength());
  EXPECT_TRUE(wrapper->IsAcceptRangesBytes());
  EXPECT_TRUE(wrapper->IsContentEncoded());
  EXPECT_EQ("application/pdf", wrapper->GetContentType());
  EXPECT_EQ("inline; filename=test.pdf", wrapper->GetContentDisposition());
  EXPECT_FALSE(wrapper->IsMultipart());
}

TEST_F(URLLoaderWrapperImplTest, ParseHeadersMultipart) {
  blink::WebURLResponse response;
  response.AddHttpHeaderField("Content-Type",
                              "multipart/byteranges; boundary=something");
  auto wrapper = CreateWrapperWithResponse(response);
  EXPECT_TRUE(wrapper->IsMultipart());
}

}  // namespace chrome_pdf
