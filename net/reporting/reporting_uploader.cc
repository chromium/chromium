// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_uploader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

constexpr char kUploadContentType[] = "application/reports+json";

constexpr net::NetworkTrafficAnnotationTag kReportUploadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("reporting", R"(
        semantics {
          sender: "Reporting API"
          description:
            "The Reporting API reports various issues back to website owners "
            "to help them detect and fix problems."
          trigger:
            "Encountering issues. Examples of these issues are Content "
            "Security Policy violations and Interventions/Deprecations "
            "encountered. See draft of reporting spec here: "
            "https://wicg.github.io/reporting."
          data: "Details of the issue, depending on issue type."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

// Returns true if |request| contains any of the |allowed_values| in a response
// header field named |header|. |allowed_values| are expected to be lower-case
// and the check is case-insensitive.
bool HasHeaderValues(URLRequest* request,
                     const std::string& header,
                     const std::set<std::string>& allowed_values) {
  std::string response_headers;
  request->GetResponseHeaderByName(header, &response_headers);
  const std::vector<std::string> response_values =
      base::SplitString(base::ToLowerASCII(response_headers), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& value : response_values) {
    if (allowed_values.find(value) != allowed_values.end())
      return true;
  }
  return false;
}

ReportingUploader::Outcome ResponseCodeToOutcome(int response_code) {
  if (response_code >= 200 && response_code <= 299)
    return ReportingUploader::Outcome::SUCCESS;
  if (response_code == 410)
    return ReportingUploader::Outcome::REMOVE_ENDPOINT;
  return ReportingUploader::Outcome::FAILURE;
}

struct PendingUpload {
  enum State { CREATED, SENDING_PREFLIGHT, SENDING_PAYLOAD };

  PendingUpload(const url::Origin& report_origin,
                const GURL& url,
                const IsolationInfo& isolation_info,
                const std::string& json,
                int max_depth,
                ReportingUploader::UploadCallback callback)
      : report_origin(report_origin),
        url(url),
        isolation_info(isolation_info),
        payload_reader(UploadOwnedBytesElementReader::CreateWithString(json)),
        max_depth(max_depth),
        callback(std::move(callback)) {}

  void RunCallback(ReportingUploader::Outcome outcome) {
    std::move(callback).Run(outcome);
  }

  State state = CREATED;
  const url::Origin report_origin;
  const GURL url;
  const IsolationInfo isolation_info;
  std::unique_ptr<UploadElementReader> payload_reader;
  int max_depth;
  ReportingUploader::UploadCallback callback;
  std::unique_ptr<URLRequest> request;
};

class ReportingUploaderImpl : public ReportingUploader, URLRequest::Delegate {
 public:
  explicit ReportingUploaderImpl(const URLRequestContext* context)
      : context_(context) {
    DCHECK(context_);
  }

  ~ReportingUploaderImpl() override {
    for (auto& request_and_upload : uploads_) {
      auto& upload = request_and_upload.second;
      upload->RunCallback(Outcome::FAILURE);
    }
  }

  void StartUpload(const url::Origin& report_origin,
                   const GURL& url,
                   const IsolationInfo& isolation_info,
                   const std::string& json,
                   int max_depth,
                   bool eligible_for_credentials,
                   UploadCallback callback) override {
    auto upload =
        std::make_unique<PendingUpload>(report_origin, url, isolation_info,
                                        json, max_depth, std::move(callback));
    auto collector_origin = url::Origin::Create(url);
    if (collector_origin == report_origin) {
      // Skip the preflight check if the reports are being sent to the same
      // origin as the requests they describe.
      StartPayloadRequest(std::move(upload), eligible_for_credentials);
    } else {
      StartPreflightRequest(std::move(upload));
    }
  }

  void OnShutdown() override {
    // Cancels all pending uploads.
    uploads_.clear();
  }

  void StartPreflightRequest(std::unique_ptr<PendingUpload> upload) {
    DCHECK(upload->state == PendingUpload::CREATED);

    upload->state = PendingUpload::SENDING_PREFLIGHT;
    upload->request = context_->CreateRequest(upload->url, IDLE, this,
                                              kReportUploadTrafficAnnotation);

    upload->request->set_method("OPTIONS");

    upload->request->SetLoadFlags(LOAD_DISABLE_CACHE);
    upload->request->set_allow_credentials(false);
    upload->request->set_isolation_info(upload->isolation_info);

    upload->request->SetExtraRequestHeaderByName(
        HttpRequestHeaders::kOrigin, upload->report_origin.Serialize(), true);
    upload->request->SetExtraRequestHeaderByName(
        "Access-Control-Request-Method", "POST", true);
    upload->request->SetExtraRequestHeaderByName(
        "Access-Control-Request-Headers", "content-type", true);

    // Set the max_depth for this request, to cap how deep a stack of "reports
    // about reports" can get.  (Without this, a Reporting policy that uploads
    // reports to the same origin can cause an infinite stack of reports about
    // reports.)
    upload->request->set_reporting_upload_depth(upload->max_depth + 1);

    URLRequest* raw_request = upload->request.get();
    uploads_[raw_request] = std::move(upload);
    raw_request->Start();
  }

  void StartPayloadRequest(std::unique_ptr<PendingUpload> upload,
                           bool eligible_for_credentials) {
    DCHECK(upload->state == PendingUpload::CREATED ||
           upload->state == PendingUpload::SENDING_PREFLIGHT);

    upload->state = PendingUpload::SENDING_PAYLOAD;
    upload->request = context_->CreateRequest(upload->url, IDLE, this,
                                              kReportUploadTrafficAnnotation);
    upload->request->set_method("POST");

    upload->request->SetLoadFlags(LOAD_DISABLE_CACHE);

    // Credentials are sent for V1 reports, if the endpoint is same-origin with
    // the site generating the report (this will be set to false either by the
    // delivery agent determining that this is a V0 report, or by `StartUpload`
    // determining that this is a cross-origin case, and taking the CORS
    // preflight path).
    upload->request->set_allow_credentials(eligible_for_credentials);
    // The site for cookies is taken from the reporting source's IsolationInfo,
    // in the case of V1 reporting endpoints, and will be null for V0 reports.
    upload->request->set_site_for_cookies(
        upload->isolation_info.site_for_cookies());
    // Prior to using `isolation_info` directly here we built the
    // `upload->network_anonymization_key` to create the set the
    // `isolation_info`. As experiments roll out to determine whether network
    // partitions should be double or triple keyed the isolation_info might have
    // a null value for `frame_origin`. Thus we should again get it from
    // `network_anonymization_key` until we can trust
    // `isolation_info::frame_origin`.
    upload->request->set_initiator(upload->report_origin);
    upload->request->set_isolation_info(upload->isolation_info);

    upload->request->SetExtraRequestHeaderByName(
        HttpRequestHeaders::kContentType, kUploadContentType, true);

    upload->request->set_upload(ElementsUploadDataStream::CreateWithReader(
        std::move(upload->payload_reader)));

    // Set the max_depth for this request, to cap how deep a stack of "reports
    // about reports" can get.  (Without this, a Reporting policy that uploads
    // reports to the same origin can cause an infinite stack of reports about
    // reports.)
    upload->request->set_reporting_upload_depth(upload->max_depth + 1);

    URLRequest* raw_request = upload->request.get();
    uploads_[raw_request] = std::move(upload);
    raw_request->Start();
  }

  // URLRequest::Delegate implementation:

  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override {
    if (!redirect_info.new_url.SchemeIsCryptographic()) {
      request->Cancel();
      return;
    }
  }

  void OnAuthRequired(URLRequest* request,
                      const AuthChallengeInfo& auth_info) override {
    request->Cancel();
  }

  void OnCertificateRequested(URLRequest* request,
                              SSLCertRequestInfo* cert_request_info) override {
    request->Cancel();
  }

  void OnSSLCertificateError(URLRequest* request,
                             int net_error,
                             const SSLInfo& ssl_info,
                             bool fatal) override {
    request->Cancel();
  }

  void OnResponseStarted(URLRequest* request, int net_error) override {
    // Grab Upload from map, and hold on to it in a local unique_ptr so it's
    // removed at the end of the method.
    auto it = uploads_.find(request);
    CHECK(it != uploads_.end(), base::NotFatalUntil::M130);
    std::unique_ptr<PendingUpload> upload = std::move(it->second);
    uploads_.erase(it);

    if (net_error != OK) {
      upload->RunCallback(ReportingUploader::Outcome::FAILURE);
      return;
    }

    // request->GetResponseCode() should work, but doesn't in the cases above
    // where the request was canceled, so get the response code by hand.
    // TODO(juliatuttle): Check if mmenke fixed this yet.
    HttpResponseHeaders* headers = request->response_headers();
    int response_code = headers ? headers->response_code() : 0;

    switch (upload->state) {
      case PendingUpload::SENDING_PREFLIGHT:
        HandlePreflightResponse(std::move(upload), response_code);
        break;
      case PendingUpload::SENDING_PAYLOAD:
        HandlePayloadResponse(std::move(upload), response_code);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  void HandlePreflightResponse(std::unique_ptr<PendingUpload> upload,
                               int response_code) {
    // Check that the preflight succeeded: it must have an HTTP OK status code,
    // with the following headers:
    // - Access-Control-Allow-Origin: * or the report origin
    // - Access-Control-Allow-Headers: * or Content-Type
    // Note that * is allowed here as the credentials mode is never 'include'.
    // Access-Control-Allow-Methods is not checked, as the preflight is always
    // for a POST method, which is safelisted.
    URLRequest* request = upload->request.get();
    bool preflight_succeeded =
        (response_code >= 200 && response_code <= 299) &&
        HasHeaderValues(
            request, "Access-Control-Allow-Origin",
            {"*", base::ToLowerASCII(upload->report_origin.Serialize())}) &&
        HasHeaderValues(request, "Access-Control-Allow-Headers",
                        {"*", "content-type"});
    if (!preflight_succeeded) {
      upload->RunCallback(ReportingUploader::Outcome::FAILURE);
      return;
    }
    // Any upload which required CORS should not receive credentials, as they
    // are sent to same-origin endpoints only.
    StartPayloadRequest(std::move(upload), /*eligible_for_credentials=*/false);
  }

  void HandlePayloadResponse(std::unique_ptr<PendingUpload> upload,
                             int response_code) {
    upload->RunCallback(ResponseCodeToOutcome(response_code));
  }

  void OnReadCompleted(URLRequest* request, int bytes_read) override {
    // Reporting doesn't need anything in the body of the response, so it
    // doesn't read it, so it should never get OnReadCompleted calls.
    NOTREACHED_IN_MIGRATION();
  }

  int GetPendingUploadCountForTesting() const override {
    return uploads_.size();
  }

 private:
  raw_ptr<const URLRequestContext> context_;
  std::map<const URLRequest*, std::unique_ptr<PendingUpload>> uploads_;
};

}  // namespace

ReportingUploader::~ReportingUploader() = default;

// static
std::unique_ptr<ReportingUploader> ReportingUploader::Create(
    const URLRequestContext* context) {
  return std::make_unique<ReportingUploaderImpl>(context);
}

}  // namespace net
