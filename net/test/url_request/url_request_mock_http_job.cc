// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_mock_http_job.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

namespace {

const char kMockHostname[] = "mock.http";
const base::FilePath::CharType kMockHeaderFileSuffix[] =
    FILE_PATH_LITERAL(".mock-http-headers");

class MockJobInterceptor : public URLRequestInterceptor {
 public:
  // When |map_all_requests_to_base_path| is true, all request should return the
  // contents of the file at |base_path|. When |map_all_requests_to_base_path|
  // is false, |base_path| is the file path leading to the root of the directory
  // to use as the root of the HTTP server.
  MockJobInterceptor(const base::FilePath& base_path,
                     bool map_all_requests_to_base_path)
      : base_path_(base_path),
        map_all_requests_to_base_path_(map_all_requests_to_base_path) {}

  MockJobInterceptor(const MockJobInterceptor&) = delete;
  MockJobInterceptor& operator=(const MockJobInterceptor&) = delete;

  ~MockJobInterceptor() override = default;

  // URLRequestJobFactory::ProtocolHandler implementation
  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
      URLRequest* request) const override {
    return std::make_unique<URLRequestMockHTTPJob>(
        request,
        map_all_requests_to_base_path_ ? base_path_ : GetOnDiskPath(request));
  }

 private:
  base::FilePath GetOnDiskPath(URLRequest* request) const {
    // Conceptually we just want to "return base_path_ + request->url().path()".
    // But path in the request URL is in URL space (i.e. %-encoded spaces).
    // So first we convert base FilePath to a URL, then append the URL
    // path to that, and convert the final URL back to a FilePath.
    GURL file_url(FilePathToFileURL(base_path_));
    std::string url = file_url.spec() + request->url().path();
    base::FilePath file_path;
    FileURLToFilePath(GURL(url), &file_path);
    return file_path;
  }

  const base::FilePath base_path_;
  const bool map_all_requests_to_base_path_;
};

std::string DoFileIO(const base::FilePath& file_path) {
  base::FilePath header_file =
      base::FilePath(file_path.value() + kMockHeaderFileSuffix);

  if (!base::PathExists(header_file)) {
    // If there is no mock-http-headers file, fake a 200 OK.
    return "HTTP/1.0 200 OK\n";
  }

  std::string raw_headers;
  base::ReadFileToString(header_file, &raw_headers);
  return raw_headers;
}

// For a given file |path| and |scheme|, return the URL served by the
// URlRequestMockHTTPJob.
GURL GetMockUrlForScheme(const std::string& path, const std::string& scheme) {
  return GURL(scheme + "://" + kMockHostname + "/" + path);
}

}  // namespace

// static
void URLRequestMockHTTPJob::AddUrlHandlers(const base::FilePath& base_path) {
  // Add kMockHostname to URLRequestFilter, for both HTTP and HTTPS.
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor("http", kMockHostname,
                                 CreateInterceptor(base_path));
  filter->AddHostnameInterceptor("https", kMockHostname,
                                 CreateInterceptor(base_path));
}

// static
GURL URLRequestMockHTTPJob::GetMockUrl(const std::string& path) {
  return GetMockUrlForScheme(path, "http");
}

// static
GURL URLRequestMockHTTPJob::GetMockHttpsUrl(const std::string& path) {
  return GetMockUrlForScheme(path, "https");
}

// static
std::unique_ptr<URLRequestInterceptor> URLRequestMockHTTPJob::CreateInterceptor(
    const base::FilePath& base_path) {
  return std::make_unique<MockJobInterceptor>(base_path, false);
}

// static
std::unique_ptr<URLRequestInterceptor>
URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
    const base::FilePath& file) {
  return std::make_unique<MockJobInterceptor>(file, true);
}

URLRequestMockHTTPJob::URLRequestMockHTTPJob(URLRequest* request,
                                             const base::FilePath& file_path)
    : URLRequestTestJobBackedByFile(
          request,
          file_path,
          base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

URLRequestMockHTTPJob::~URLRequestMockHTTPJob() = default;

// Public virtual version.
void URLRequestMockHTTPJob::GetResponseInfo(HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

bool URLRequestMockHTTPJob::IsRedirectResponse(
    GURL* location,
    int* http_status_code,
    bool* insecure_scheme_was_upgraded) {
  // Override the URLRequestTestJobBackedByFile implementation to invoke the
  // default one based on HttpResponseInfo.
  return URLRequestJob::IsRedirectResponse(location, http_status_code,
                                           insecure_scheme_was_upgraded);
}

void URLRequestMockHTTPJob::OnReadComplete(net::IOBuffer* buffer, int result) {
  if (result >= 0)
    total_received_bytes_ += result;
}

// Public virtual version.
void URLRequestMockHTTPJob::Start() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&DoFileIO, file_path_),
      base::BindOnce(&URLRequestMockHTTPJob::SetHeadersAndStart,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLRequestMockHTTPJob::SetHeadersAndStart(const std::string& raw_headers) {
  raw_headers_ = raw_headers;
  // Handle CRLF line-endings.
  base::ReplaceSubstringsAfterOffset(&raw_headers_, 0, "\r\n", "\n");
  // ParseRawHeaders expects \0 to end each header line.
  base::ReplaceSubstringsAfterOffset(&raw_headers_, 0, "\n",
                                     std::string_view("\0", 1));
  total_received_bytes_ += raw_headers_.size();
  URLRequestTestJobBackedByFile::Start();
}

// Private const version.
void URLRequestMockHTTPJob::GetResponseInfoConst(HttpResponseInfo* info) const {
  info->headers = base::MakeRefCounted<HttpResponseHeaders>(raw_headers_);
}

int64_t URLRequestMockHTTPJob::GetTotalReceivedBytes() const {
  return total_received_bytes_;
}

bool URLRequestMockHTTPJob::GetMimeType(std::string* mime_type) const {
  HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers.get() && info.headers->GetMimeType(mime_type);
}

bool URLRequestMockHTTPJob::GetCharset(std::string* charset) {
  HttpResponseInfo info;
  GetResponseInfo(&info);
  return info.headers.get() && info.headers->GetCharset(charset);
}

}  // namespace net
