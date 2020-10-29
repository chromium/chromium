// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/expect_ct_reporter.h"

#include <set>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/cert/ct_serialization.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/features.h"

namespace network {
namespace {

// Returns true if |request| contains any of the |allowed_values| in a response
// header field named |header|. |allowed_values| are expected to be lower-case
// and the check is case-insensitive.
bool HasHeaderValues(net::URLRequest* request,
                     const std::string& header,
                     const std::set<std::string>& allowed_values) {
  std::string response_headers;
  request->GetResponseHeaderByName(header, &response_headers);
  const std::vector<std::string> response_values = base::SplitString(
      response_headers, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& value : response_values) {
    for (const auto& allowed : allowed_values) {
      if (base::ToLowerASCII(allowed) == base::ToLowerASCII(value)) {
        return true;
      }
    }
  }
  return false;
}

std::unique_ptr<base::ListValue> GetPEMEncodedChainAsList(
    const net::X509Certificate* cert_chain) {
  if (!cert_chain)
    return std::make_unique<base::ListValue>();

  std::unique_ptr<base::ListValue> result(new base::ListValue());
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  for (const std::string& cert : pem_encoded_chain)
    result->Append(std::make_unique<base::Value>(cert));

  return result;
}

std::string SCTOriginToString(
    net::ct::SignedCertificateTimestamp::Origin origin) {
  switch (origin) {
    case net::ct::SignedCertificateTimestamp::SCT_EMBEDDED:
      return "embedded";
    case net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION:
      return "tls-extension";
    case net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE:
      return "ocsp";
    case net::ct::SignedCertificateTimestamp::SCT_ORIGIN_MAX:
      NOTREACHED();
  }
  return "";
}

bool AddSCT(const net::SignedCertificateTimestampAndStatus& sct,
            base::ListValue* list) {
  std::unique_ptr<base::DictionaryValue> list_item(new base::DictionaryValue());
  // Chrome implements RFC6962, not 6962-bis, so the reports contain v1 SCTs.
  list_item->SetInteger("version", 1);
  std::string status;
  switch (sct.status) {
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
      status = "unknown";
      break;
    case net::ct::SCT_STATUS_INVALID_SIGNATURE:
    case net::ct::SCT_STATUS_INVALID_TIMESTAMP:
      status = "invalid";
      break;
    case net::ct::SCT_STATUS_OK:
      status = "valid";
      break;
    case net::ct::SCT_STATUS_NONE:
      NOTREACHED();
  }
  list_item->SetString("status", status);
  list_item->SetString("source", SCTOriginToString(sct.sct->origin));
  std::string serialized_sct;
  if (!net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct))
    return false;
  std::string encoded_serialized_sct;
  base::Base64Encode(serialized_sct, &encoded_serialized_sct);
  list_item->SetString("serialized_sct", encoded_serialized_sct);
  list->Append(std::move(list_item));
  return true;
}

constexpr net::NetworkTrafficAnnotationTag kExpectCTReporterTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("expect_ct_reporter", R"(
        semantics {
          sender: "Expect-CT reporting for Certificate Transparency reporting"
          description:
            "Websites can opt in to have Chrome send reports to them when "
            "Chrome observes connections to that website that do not meet "
            "Chrome's Certificate Transparency policy. Websites can use this "
            "feature to discover misconfigurations that prevent them from "
            "complying with Chrome's Certificate Transparency policy."
          trigger: "Website request."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and the Signed Certificate Timestamps "
            "observed on the connection."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, this is a feature that websites can opt into and "
            "thus there is no Chrome-wide policy to disable it."
        })");

}  // namespace

ExpectCTReporter::ExpectCTReporter(
    net::URLRequestContext* request_context,
    const base::RepeatingClosure& success_callback,
    const base::RepeatingClosure& failure_callback)
    : report_sender_(new net::ReportSender(request_context,
                                           kExpectCTReporterTrafficAnnotation)),
      request_context_(request_context),
      success_callback_(success_callback),
      failure_callback_(failure_callback) {}

ExpectCTReporter::~ExpectCTReporter() {}

void ExpectCTReporter::OnExpectCTFailed(
    const net::HostPortPair& host_port_pair,
    const GURL& report_uri,
    base::Time expiration,
    const net::X509Certificate* validated_certificate_chain,
    const net::X509Certificate* served_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps,
    const net::NetworkIsolationKey& network_isolation_key) {
  if (report_uri.is_empty())
    return;

  if (!base::FeatureList::IsEnabled(features::kExpectCTReporting))
    return;

  base::DictionaryValue outer_report;
  base::DictionaryValue* report = outer_report.SetDictionary(
      "expect-ct-report", std::make_unique<base::DictionaryValue>());
  report->SetString("hostname", host_port_pair.host());
  report->SetInteger("port", host_port_pair.port());
  report->SetString("date-time", base::TimeToISO8601(base::Time::Now()));
  report->SetString("effective-expiration-date",
                    base::TimeToISO8601(expiration));
  report->Set("served-certificate-chain",
              GetPEMEncodedChainAsList(served_certificate_chain));
  report->Set("validated-certificate-chain",
              GetPEMEncodedChainAsList(validated_certificate_chain));

  std::unique_ptr<base::ListValue> scts(new base::ListValue());
  for (const auto& sct_and_status : signed_certificate_timestamps) {
    if (!AddSCT(sct_and_status, scts.get()))
      LOG(ERROR) << "Failed to add signed certificate timestamp to list";
  }
  report->Set("scts", std::move(scts));

  std::string serialized_report;
  if (!base::JSONWriter::Write(outer_report, &serialized_report)) {
    LOG(ERROR) << "Failed to serialize Expect CT report";
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("SSL.ExpectCTReportSendingAttempt", true);

  SendPreflight(report_uri, serialized_report, network_isolation_key);
}

void ExpectCTReporter::OnResponseStarted(net::URLRequest* request,
                                         int net_error) {
  auto preflight_it = inflight_preflights_.find(request);
  DCHECK(inflight_preflights_.end() != inflight_preflights_.find(request));
  PreflightInProgress* preflight = preflight_it->second.get();

  const int response_code =
      net_error == net::OK ? request->GetResponseCode() : -1;

  // Check that the preflight succeeded: it must have an HTTP OK status code,
  // with the following headers:
  // - Access-Control-Allow-Origin: * or null
  // - Access-Control-Allow-Methods: POST
  // - Access-Control-Allow-Headers: Content-Type

  if (response_code == -1 || response_code < 200 || response_code > 299) {
    OnReportFailure(preflight->report_uri, net_error, response_code);
    inflight_preflights_.erase(request);
    // Do not use |preflight| after this point, since it has been erased above.
    return;
  }

  if (!HasHeaderValues(request, "Access-Control-Allow-Origin", {"*", "null"}) ||
      !HasHeaderValues(request, "Access-Control-Allow-Methods", {"post"}) ||
      !HasHeaderValues(request, "Access-Control-Allow-Headers",
                       {"content-type"})) {
    OnReportFailure(preflight->report_uri, net_error, response_code);
    inflight_preflights_.erase(request);
    // Do not use |preflight| after this point, since it has been erased above.
    return;
  }

  report_sender_->Send(preflight->report_uri,
                       "application/expect-ct-report+json; charset=utf-8",
                       preflight->serialized_report,
                       preflight->network_isolation_key, success_callback_,
                       // Since |this| owns the |report_sender_|, it's safe to
                       // use base::Unretained here: |report_sender_| will be
                       // destroyed before |this|.
                       base::BindRepeating(&ExpectCTReporter::OnReportFailure,
                                           base::Unretained(this)));
  inflight_preflights_.erase(request);
}

void ExpectCTReporter::OnReadCompleted(net::URLRequest* request,
                                       int bytes_read) {
  NOTREACHED();
}

ExpectCTReporter::PreflightInProgress::PreflightInProgress(
    std::unique_ptr<net::URLRequest> request,
    const std::string& serialized_report,
    const GURL& report_uri,
    const net::NetworkIsolationKey& network_isolation_key)
    : request(std::move(request)),
      serialized_report(serialized_report),
      report_uri(report_uri),
      network_isolation_key(network_isolation_key) {}

ExpectCTReporter::PreflightInProgress::~PreflightInProgress() {}

void ExpectCTReporter::SendPreflight(
    const GURL& report_uri,
    const std::string& serialized_report,
    const net::NetworkIsolationKey& network_isolation_key) {
  std::unique_ptr<net::URLRequest> url_request =
      request_context_->CreateRequest(report_uri, net::DEFAULT_PRIORITY, this,
                                      kExpectCTReporterTrafficAnnotation);
  url_request->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE);
  url_request->set_allow_credentials(false);
  url_request->set_method(net::HttpRequestHeaders::kOptionsMethod);
  url_request->set_isolation_info(net::IsolationInfo::CreatePartial(
      net::IsolationInfo::RequestType::kOther, network_isolation_key));

  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader("Origin", "null");
  extra_headers.SetHeader("Access-Control-Request-Method",
                          net::HttpRequestHeaders::kPostMethod);
  extra_headers.SetHeader("Access-Control-Request-Headers", "content-type");
  url_request->SetExtraRequestHeaders(extra_headers);

  net::URLRequest* raw_request = url_request.get();
  inflight_preflights_[raw_request] = std::make_unique<PreflightInProgress>(
      std::move(url_request), serialized_report, report_uri,
      network_isolation_key);
  raw_request->Start();
}

void ExpectCTReporter::OnReportFailure(const GURL& report_uri,
                                       int net_error,
                                       int http_response_code) {
  base::UmaHistogramSparse("SSL.ExpectCTReportFailure2", -net_error);
  if (!failure_callback_.is_null())
    failure_callback_.Run();
}

}  // namespace network
