// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_fetcher_core.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_fetcher_response_writer.h"

namespace net {

static URLFetcherFactory* g_factory = nullptr;

URLFetcherImpl::URLFetcherImpl(
    const GURL& url,
    RequestType request_type,
    URLFetcherDelegate* d,
    net::NetworkTrafficAnnotationTag traffic_annotation)
    : core_(
          new URLFetcherCore(this, url, request_type, d, traffic_annotation)) {}

URLFetcherImpl::~URLFetcherImpl() {
  core_->Stop();
}

void URLFetcherImpl::SetUploadData(const std::string& upload_content_type,
                                   const std::string& upload_content) {
  core_->SetUploadData(upload_content_type, upload_content);
}

void URLFetcherImpl::SetUploadFilePath(
    const std::string& upload_content_type,
    const base::FilePath& file_path,
    uint64_t range_offset,
    uint64_t range_length,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  core_->SetUploadFilePath(upload_content_type,
                           file_path,
                           range_offset,
                           range_length,
                           file_task_runner);
}

void URLFetcherImpl::SetUploadStreamFactory(
    const std::string& upload_content_type,
    const CreateUploadStreamCallback& callback) {
  core_->SetUploadStreamFactory(upload_content_type, callback);
}

void URLFetcherImpl::SetChunkedUpload(const std::string& content_type) {
  core_->SetChunkedUpload(content_type);
}

void URLFetcherImpl::AppendChunkToUpload(const std::string& data,
                                         bool is_last_chunk) {
  DCHECK(data.length());
  core_->AppendChunkToUpload(data, is_last_chunk);
}

void URLFetcherImpl::SetReferrer(const std::string& referrer) {
  core_->SetReferrer(referrer);
}

void URLFetcherImpl::SetReferrerPolicy(
    URLRequest::ReferrerPolicy referrer_policy) {
  core_->SetReferrerPolicy(referrer_policy);
}

void URLFetcherImpl::SetLoadFlags(int load_flags) {
  core_->SetLoadFlags(load_flags);
}

void URLFetcherImpl::SetAllowCredentials(bool allow_credentials) {
  core_->SetAllowCredentials(allow_credentials);
}

int URLFetcherImpl::GetLoadFlags() const {
  return core_->GetLoadFlags();
}

void URLFetcherImpl::SetExtraRequestHeaders(
    const std::string& extra_request_headers) {
  core_->SetExtraRequestHeaders(extra_request_headers);
}

void URLFetcherImpl::AddExtraRequestHeader(const std::string& header_line) {
  core_->AddExtraRequestHeader(header_line);
}

void URLFetcherImpl::SetRequestContext(
    URLRequestContextGetter* request_context_getter) {
  core_->SetRequestContext(request_context_getter);
}

void URLFetcherImpl::SetInitiator(
    const base::Optional<url::Origin>& initiator) {
  core_->SetInitiator(initiator);
}

void URLFetcherImpl::SetURLRequestUserData(
    const void* key,
    const CreateDataCallback& create_data_callback) {
  core_->SetURLRequestUserData(key, create_data_callback);
}

void URLFetcherImpl::SetStopOnRedirect(bool stop_on_redirect) {
  core_->SetStopOnRedirect(stop_on_redirect);
}

void URLFetcherImpl::SetAutomaticallyRetryOn5xx(bool retry) {
  core_->SetAutomaticallyRetryOn5xx(retry);
}

void URLFetcherImpl::SetMaxRetriesOn5xx(int max_retries) {
  core_->SetMaxRetriesOn5xx(max_retries);
}

int URLFetcherImpl::GetMaxRetriesOn5xx() const {
  return core_->GetMaxRetriesOn5xx();
}


base::TimeDelta URLFetcherImpl::GetBackoffDelay() const {
  return core_->GetBackoffDelay();
}

void URLFetcherImpl::SetAutomaticallyRetryOnNetworkChanges(int max_retries) {
  core_->SetAutomaticallyRetryOnNetworkChanges(max_retries);
}

void URLFetcherImpl::SaveResponseToFileAtPath(
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  core_->SaveResponseToFileAtPath(file_path, file_task_runner);
}

void URLFetcherImpl::SaveResponseToTemporaryFile(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  core_->SaveResponseToTemporaryFile(file_task_runner);
}

void URLFetcherImpl::SaveResponseWithWriter(
    std::unique_ptr<URLFetcherResponseWriter> response_writer) {
  core_->SaveResponseWithWriter(std::move(response_writer));
}

HttpResponseHeaders* URLFetcherImpl::GetResponseHeaders() const {
  return core_->GetResponseHeaders();
}

IPEndPoint URLFetcherImpl::GetSocketAddress() const {
  return core_->GetSocketAddress();
}

const ProxyServer& URLFetcherImpl::ProxyServerUsed() const {
  return core_->ProxyServerUsed();
}

bool URLFetcherImpl::WasCached() const {
  return core_->WasCached();
}

int64_t URLFetcherImpl::GetReceivedResponseContentLength() const {
  return core_->GetReceivedResponseContentLength();
}

int64_t URLFetcherImpl::GetTotalReceivedBytes() const {
  return core_->GetTotalReceivedBytes();
}

void URLFetcherImpl::Start() {
  core_->Start();
}

const GURL& URLFetcherImpl::GetOriginalURL() const {
  return core_->GetOriginalURL();
}

const GURL& URLFetcherImpl::GetURL() const {
  return core_->GetURL();
}

const URLRequestStatus& URLFetcherImpl::GetStatus() const {
  return core_->GetStatus();
}

int URLFetcherImpl::GetResponseCode() const {
  return core_->GetResponseCode();
}

void URLFetcherImpl::ReceivedContentWasMalformed() {
  core_->ReceivedContentWasMalformed();
}

bool URLFetcherImpl::GetResponseAsString(
    std::string* out_response_string) const {
  return core_->GetResponseAsString(out_response_string);
}

bool URLFetcherImpl::GetResponseAsFilePath(
    bool take_ownership,
    base::FilePath* out_response_path) const {
  return core_->GetResponseAsFilePath(take_ownership, out_response_path);
}

// static
void URLFetcherImpl::CancelAll() {
  URLFetcherCore::CancelAll();
}

// static
void URLFetcherImpl::SetIgnoreCertificateRequests(bool ignored) {
  URLFetcherCore::SetIgnoreCertificateRequests(ignored);
}

// static
int URLFetcherImpl::GetNumFetcherCores() {
  return URLFetcherCore::GetNumFetcherCores();
}

URLFetcherDelegate* URLFetcherImpl::delegate() const {
  return core_->delegate();
}

// static
URLFetcherFactory* URLFetcherImpl::factory() {
  return g_factory;
}

// static
void URLFetcherImpl::set_factory(URLFetcherFactory* factory) {
  g_factory = factory;
}

}  // namespace net
