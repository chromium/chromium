// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/download/download_request_core.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kTestLastModifiedTime[] = "Tue, 15 Nov 1994 12:45:26 GMT";

}  // namespace

class DownloadRequestCoreTest : public testing::Test {
 public:
  std::unique_ptr<download::DownloadUrlParameters> BuildDownloadParameters(
      const std::string& url) const {
    GURL gurl(url);
    return std::make_unique<download::DownloadUrlParameters>(
        gurl, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void CheckRequestHeaders(const std::string& name,
                           const std::string& expected_header_value) const {
    DCHECK(url_request_.get());
    std::string header_value;
    url_request_->extra_request_headers().GetHeader(name, &header_value);
    EXPECT_EQ(expected_header_value, header_value);
  }

  bool HasRequestHeader(const std::string& name) {
    DCHECK(url_request_.get());
    return url_request_->extra_request_headers().HasHeader(name);
  }

  void CreateRequestOnIOThread(download::DownloadUrlParameters* params) {
    url_request_ = DownloadRequestCore::CreateRequestOnIOThread(
        true, params, request_context_getter_);
    DCHECK(url_request_.get());
  }

  void SetUp() override {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI}));
  }

  void TearDown() override {
    // URLRequest must be released before |request_context_getter_| gets
    // destroyed.
    url_request_.reset();
  }

  std::unique_ptr<net::URLRequest> url_request_;

  // Used to test functions run on particular browser thread.
  content::TestBrowserThreadBundle browser_threads_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
};

// Ensure "Range" header is built correctly for normal download.
TEST_F(DownloadRequestCoreTest, BuildRangeRequest) {
  std::unique_ptr<download::DownloadUrlParameters> params =
      BuildDownloadParameters("example.com");

  // Check initial states.
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent, params->length());
  EXPECT_EQ(0, params->offset());
  EXPECT_TRUE(params->use_if_range());

  // Non-range request.
  CreateRequestOnIOThread(params.get());
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kRange));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfRange));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfMatch));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfUnmodifiedSince));
  url_request_.reset();

  // Range request with header "Range:bytes=50-99", and etag.
  params = BuildDownloadParameters("example.com");
  params->set_etag("123");
  params->set_offset(50);
  params->set_length(50);
  params->set_use_if_range(false);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=50-99");
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfRange));
  CheckRequestHeaders(net::HttpRequestHeaders::kIfMatch, "123");
  CheckRequestHeaders(net::HttpRequestHeaders::kIfUnmodifiedSince, "");
  url_request_.reset();

  // Range request with header "Range:bytes=0-49" and last modified time.
  params = BuildDownloadParameters("example.com");
  params->set_etag("");
  params->set_last_modified(kTestLastModifiedTime);
  params->set_offset(0);
  params->set_length(50);
  params->set_use_if_range(false);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=0-49");
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfRange));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfMatch));
  CheckRequestHeaders(net::HttpRequestHeaders::kIfUnmodifiedSince,
                      kTestLastModifiedTime);
  url_request_.reset();

  // Range request with header "Range:bytes=10-59" and includes both etag and
  // last modified time.
  params = BuildDownloadParameters("example.com");
  params->set_etag("123");
  params->set_last_modified(kTestLastModifiedTime);
  params->set_offset(10);
  params->set_length(50);
  params->set_use_if_range(false);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=10-59");
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfRange));
  CheckRequestHeaders(net::HttpRequestHeaders::kIfMatch, "123");
  CheckRequestHeaders(net::HttpRequestHeaders::kIfUnmodifiedSince,
                      kTestLastModifiedTime);
  url_request_.reset();

  // Range request with header "Range:bytes=10-59" and use "If-Range"
  // header.
  params = BuildDownloadParameters("example.com");
  params->set_etag("123");
  params->set_last_modified(kTestLastModifiedTime);
  params->set_offset(10);
  params->set_length(50);
  params->set_use_if_range(true);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=10-59");
  CheckRequestHeaders(net::HttpRequestHeaders::kIfRange, "123");
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfMatch));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfUnmodifiedSince));
  url_request_.reset();
}

// Ensure "Range" header is built correctly for download resumption.
// Notice download resumption requires strong validator(i.e. etag or
// last-modified).
TEST_F(DownloadRequestCoreTest, BuildRangeRequestWithoutLength) {
  std::unique_ptr<download::DownloadUrlParameters> params =
      BuildDownloadParameters("example.com");
  params->set_etag("123");
  params->set_offset(50);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=50-");
  CheckRequestHeaders(net::HttpRequestHeaders::kIfRange, "123");
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfMatch));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfUnmodifiedSince));
  url_request_.reset();

  params = BuildDownloadParameters("example.com");
  params->set_last_modified(kTestLastModifiedTime);
  params->set_offset(50);
  params->set_use_if_range(false);
  CreateRequestOnIOThread(params.get());
  CheckRequestHeaders(net::HttpRequestHeaders::kRange, "bytes=50-");
  CheckRequestHeaders(net::HttpRequestHeaders::kIfUnmodifiedSince,
                      kTestLastModifiedTime);
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfRange));
  EXPECT_FALSE(HasRequestHeader(net::HttpRequestHeaders::kIfMatch));
  url_request_.reset();
}

}  // namespace content
