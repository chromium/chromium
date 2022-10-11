// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/report_sender.h"

#include <utility>

#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace {
const void* const kUserDataKey = &kUserDataKey;

class CallbackInfo : public base::SupportsUserData::Data {
 public:
  CallbackInfo(net::ReportSender::SuccessCallback success_callback,
               net::ReportSender::ErrorCallback error_callback)
      : success_callback_(std::move(success_callback)),
        error_callback_(std::move(error_callback)) {}

  ~CallbackInfo() override = default;

  void RunSuccessCallback() {
    if (!success_callback_.is_null())
      std::move(success_callback_).Run();
  }

  void RunErrorCallback(const GURL& url,
                        int net_error,
                        int http_response_code) {
    if (!error_callback_.is_null())
      std::move(error_callback_).Run(url, net_error, http_response_code);
  }

 private:
  net::ReportSender::SuccessCallback success_callback_;
  net::ReportSender::ErrorCallback error_callback_;
};
}  // namespace

namespace net {

const int ReportSender::kLoadFlags = LOAD_BYPASS_CACHE | LOAD_DISABLE_CACHE;

ReportSender::ReportSender(URLRequestContext* request_context,
                           net::NetworkTrafficAnnotationTag traffic_annotation)
    : request_context_(request_context),
      traffic_annotation_(traffic_annotation) {}

ReportSender::~ReportSender() = default;

void ReportSender::Send(
    const GURL& report_uri,
    base::StringPiece content_type,
    base::StringPiece report,
    const NetworkAnonymizationKey& network_anonymization_key,
    SuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK(!content_type.empty());
  std::unique_ptr<URLRequest> url_request = request_context_->CreateRequest(
      report_uri, DEFAULT_PRIORITY, this, traffic_annotation_);
  url_request->SetUserData(
      &kUserDataKey, std::make_unique<CallbackInfo>(std::move(success_callback),
                                                    std::move(error_callback)));
  url_request->SetLoadFlags(kLoadFlags);
  url_request->set_allow_credentials(false);
  url_request->set_isolation_info_from_network_anonymization_key(
      network_anonymization_key);

  HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(HttpRequestHeaders::kContentType, content_type);
  url_request->SetExtraRequestHeaders(extra_headers);

  url_request->set_method("POST");

  std::vector<char> report_data(report.begin(), report.end());
  auto reader = std::make_unique<UploadOwnedBytesElementReader>(&report_data);
  url_request->set_upload(
      ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));

  URLRequest* raw_url_request = url_request.get();
  inflight_requests_[raw_url_request] = std::move(url_request);
  raw_url_request->Start();
}

void ReportSender::OnResponseStarted(URLRequest* request, int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);

  CallbackInfo* callback_info =
      static_cast<CallbackInfo*>(request->GetUserData(&kUserDataKey));
  DCHECK(callback_info);
  if (net_error != OK) {
    DVLOG(1) << "Failed to send report for " << request->url().host();
    callback_info->RunErrorCallback(request->url(), net_error, -1);
  } else if (request->GetResponseCode() != net::HTTP_OK) {
    callback_info->RunErrorCallback(request->url(), OK,
                                    request->GetResponseCode());
  } else {
    callback_info->RunSuccessCallback();
  }
  CHECK_GT(inflight_requests_.erase(request), 0u);
}

void ReportSender::OnReadCompleted(URLRequest* request, int bytes_read) {
  NOTREACHED();
}

}  // namespace net
