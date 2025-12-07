// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "third_party/blink/renderer/platform/media/resource_multi_buffer_data_provider.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/format_macros.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
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

// Predicate that checks the Accept-Encoding request header.
static bool CorrectAcceptEncoding(const WebURLRequest& request) {
  std::string value = request
                          .HttpHeaderField(WebString::FromUTF8(
                              net::HttpRequestHeaders::kAcceptEncoding))
                          .Utf8();
  return (base::Contains(value, "identity;q=1")) &&
         (base::Contains(value, "*;q=0"));
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

  void Initialize(const char* url, int first_position) {
    url_ = KURL(url);
    url_data_ =
        url_index_->GetByUrl(url_, UrlData::CORS_UNSPECIFIED, UrlData::kNormal);
    url_data_->set_etag(kEtag);
    DCHECK(url_data_);
    url_data_->OnRedirect(
        blink::BindOnce(&ResourceMultiBufferDataProviderTest::RedirectCallback,
                        Unretained(this)));

    first_position_ = first_position;

    auto loader = std::make_unique<ResourceMultiBufferDataProvider>(
        url_data_.get(), first_position_, false /* is_client_audio_element */,
        task_environment_.GetMainThreadTaskRunner());
    loader_ = loader.get();
    url_data_->multibuffer()->AddProvider(std::move(loader));
  }

  void Start() { loader_->Start(); }

  void FullResponse(int64_t instance_size, bool ok = true) {
    WebURLResponse response(url_);
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
    WebURLResponse response(url_);
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

 protected:
  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) {
    auto url_loader = std::make_unique<NiceMock<MockWebAssociatedURLLoader>>();
    EXPECT_CALL(
        *url_loader.get(),
        LoadAsynchronously(Truly(CorrectAcceptEncoding), loader_.get()));
    return url_loader;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  KURL url_;
  int32_t first_position_;

  NiceMock<MockResourceFetchContext> fetch_context_;
  std::unique_ptr<UrlIndex> url_index_;
  scoped_refptr<UrlData> url_data_;
  scoped_refptr<UrlData> redirected_to_;
  // The loader is owned by the UrlData above.
  raw_ptr<ResourceMultiBufferDataProvider> loader_;

  std::array<uint8_t, kDataSize> data_;
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

// Tests partial response after a redirect.
TEST_F(ResourceMultiBufferDataProviderTest, TestRedirectedPartialResponse) {
  Initialize(kHttpUrl, 0);
  Start();
  PartialResponse(0, 2048, 32000);
  Redirect(kHttpRedirect);
  PartialResponse(2048, 4096, 32000);
  StopWhenLoad();
}

}  // namespace blink
