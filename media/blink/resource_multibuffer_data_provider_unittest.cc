// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/resource_multibuffer_data_provider.h"

#include <stdint.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "media/base/media_log.h"
#include "media/base/seekable_buffer.h"
#include "media/blink/mock_resource_fetch_context.h"
#include "media/blink/mock_webassociatedurlloader.h"
#include "media/blink/url_index.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Truly;
using ::testing::NiceMock;

using blink::WebString;
using blink::WebURLError;
using blink::WebURLResponse;

namespace media {

const char kHttpUrl[] = "http://test";
const char kHttpsUrl[] = "https://test";
const char kHttpRedirect[] = "http://test/ing";
const char kEtag[] = "\"arglebargle glopy-glyf?\"";

const int kDataSize = 1024;
const int kHttpOK = 200;
const int kHttpPartialContent = 206;

enum NetworkState { NONE, LOADED, LOADING };

static bool want_frfr = false;

// Predicate that checks the Chrome-Proxy and Accept-Encoding request headers.
static bool CorrectAcceptEncodingAndProxy(const blink::WebURLRequest& request) {
  std::string chrome_proxy =
      request.HttpHeaderField(WebString::FromUTF8("chrome-proxy")).Utf8();
  bool has_frfr = chrome_proxy == "frfr";
  if (has_frfr != want_frfr) {
    return false;
  }

  std::string value = request
                          .HttpHeaderField(WebString::FromUTF8(
                              net::HttpRequestHeaders::kAcceptEncoding))
                          .Utf8();
  return (value.find("identity;q=1") != std::string::npos) &&
         (value.find("*;q=0") != std::string::npos);
}

class ResourceMultiBufferDataProviderTest : public testing::Test {
 public:
  ResourceMultiBufferDataProviderTest()
      : url_index_(std::make_unique<UrlIndex>(&fetch_context_, 0)) {
    for (int i = 0; i < kDataSize; ++i) {
      data_[i] = i;
    }
    ON_CALL(fetch_context_, CreateUrlLoader(_))
        .WillByDefault(Invoke(
            this, &ResourceMultiBufferDataProviderTest::CreateUrlLoader));
  }

  void Initialize(const char* url, int first_position) {
    want_frfr = false;
    gurl_ = GURL(url);
    url_data_ = url_index_->GetByUrl(gurl_, UrlData::CORS_UNSPECIFIED);
    url_data_->set_etag(kEtag);
    DCHECK(url_data_);
    url_data_->OnRedirect(
        base::Bind(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                   base::Unretained(this)));

    first_position_ = first_position;

    std::unique_ptr<ResourceMultiBufferDataProvider> loader(
        new ResourceMultiBufferDataProvider(
            url_data_.get(), first_position_,
            false /* is_client_audio_element */));
    loader_ = loader.get();
    url_data_->multibuffer()->AddProvider(std::move(loader));
  }

  void Start() {
    loader_->Start();
  }

  void FullResponse(int64_t instance_size, bool ok = true) {
    WebURLResponse response(gurl_);
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Length"),
        WebString::FromUTF8(base::StringPrintf("%" PRId64, instance_size)));
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
    WebURLResponse response(gurl_);
    response.SetHttpHeaderField(
        WebString::FromUTF8("Content-Range"),
        WebString::FromUTF8(
            base::StringPrintf("bytes "
                               "%" PRId64 "-%" PRId64 "/%" PRId64,
                               first_position, last_position, instance_size)));

    // HTTP 1.1 doesn't permit Content-Length with Transfer-Encoding: chunked.
    int64_t content_length = -1;
    if (chunked) {
      response.SetHttpHeaderField(WebString::FromUTF8("Transfer-Encoding"),
                                  WebString::FromUTF8("chunked"));
    } else {
      content_length = last_position - first_position + 1;
    }
    response.SetExpectedContentLength(content_length);

    // A server isn't required to return Accept-Ranges even though it might.
    if (accept_ranges) {
      response.SetHttpHeaderField(WebString::FromUTF8("Accept-Ranges"),
                                  WebString::FromUTF8("bytes"));
    }

    response.SetHttpStatusCode(kHttpPartialContent);
    loader_->DidReceiveResponse(response);

    EXPECT_EQ(instance_size, url_data_->length());

    // A valid partial response should always result in this being true.
    EXPECT_TRUE(url_data_->range_supported());
  }

  void Redirect(const char* url) {
    blink::WebURL new_url{GURL(url)};
    blink::WebURLResponse redirect_response(gurl_);

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

  // Helper method to write to |loader_| from |data_|.
  void WriteLoader(int position, int size) {
    loader_->DidReceiveData(reinterpret_cast<char*>(data_ + position), size);
  }

  void WriteData(int size) {
    std::unique_ptr<char[]> data(new char[size]);
    loader_->DidReceiveData(data.get(), size);
  }

  // Verifies that data in buffer[0...size] is equal to data_[pos...pos+size].
  void VerifyBuffer(uint8_t* buffer, int pos, int size) {
    EXPECT_EQ(0, memcmp(buffer, data_ + pos, size));
  }

  MOCK_METHOD1(RedirectCallback, void(const scoped_refptr<UrlData>&));

  void SetUrlData(const scoped_refptr<UrlData>& new_url_data) {
    url_data_ = new_url_data;
  }

 protected:
  std::unique_ptr<blink::WebAssociatedURLLoader> CreateUrlLoader(
      const blink::WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    EXPECT_CALL(
        *url_loader.get(),
        LoadAsynchronously(Truly(CorrectAcceptEncodingAndProxy), loader_));
    return url_loader;
  }

  GURL gurl_;
  int64_t first_position_;

  NiceMock<MockResourceFetchContext> fetch_context_;
  std::unique_ptr<UrlIndex> url_index_;
  scoped_refptr<UrlData> url_data_;
  scoped_refptr<UrlData> redirected_to_;
  // The loader is owned by the UrlData above.
  ResourceMultiBufferDataProvider* loader_;

  uint8_t data_[kDataSize];

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceMultiBufferDataProviderTest);
};

TEST_F(ResourceMultiBufferDataProviderTest, StartStop) {
  Initialize(kHttpUrl, 0);
  Start();
  StopWhenLoad();
}

// Tests that a bad HTTP response is recived, e.g. file not found.
TEST_F(ResourceMultiBufferDataProviderTest, BadHttpResponse) {
  Initialize(kHttpUrl, 0);
  Start();

  EXPECT_CALL(*this, RedirectCallback(scoped_refptr<UrlData>(nullptr)));

  WebURLResponse response(gurl_);
  response.SetHttpStatusCode(404);
  response.SetHttpStatusText("Not Found\n");
  loader_->DidReceiveResponse(response);
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

  WebURLResponse response(gurl_);
  response.SetHttpHeaderField(
      WebString::FromUTF8("Content-Range"),
      WebString::FromUTF8(base::StringPrintf("bytes "
                                             "%d-%d/%d",
                                             1, 10, 1024)));
  response.SetExpectedContentLength(10);
  response.SetHttpStatusCode(kHttpPartialContent);
  loader_->DidReceiveResponse(response);
}

TEST_F(ResourceMultiBufferDataProviderTest, TestRedirects) {
  // Test redirect.
  Initialize(kHttpUrl, 0);
  Start();
  Redirect(kHttpRedirect);
  FullResponse(1024);
  StopWhenLoad();
}

TEST_F(ResourceMultiBufferDataProviderTest, TestChromeProxyHeader) {
  struct TestCase {
    std::string label;
    bool enable_save_data;
    std::string url;
    bool want_chrome_proxy;
  };
  const TestCase kTestCases[]{
      {
          "SaveData on, HTTP URL: chrome-proxy should exist.",
          true,
          kHttpUrl,
          true,
      },
      {
          "SaveData off, HTTP URL: chrome-proxy should not exist.",
          false,
          kHttpUrl,
          false,
      },
      {
          "SaveData on, HTTPS URL: chrome-proxy should not exist.",
          true,
          kHttpsUrl,
          false,
      },
      {
          "SaveData off, HTTPS URL: chrome-proxy should not exist.",
          false,
          kHttpsUrl,
          false,
      },
  };
  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.label);
    blink::WebNetworkStateNotifier::SetSaveDataEnabled(
        test_case.enable_save_data);

    Initialize(test_case.url.c_str(), 0);
    want_frfr = test_case.want_chrome_proxy;

    Start();
    FullResponse(1024);
    StopWhenLoad();
  }
}

}  // namespace media
