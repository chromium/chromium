// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_reporter.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kSXGResultOk[] = "ok";
constexpr char kSXGResultFailed[] = "sxg.failed";
constexpr char kSXGResultMiError[] = "sxg.mi_error";
constexpr char kSXGResultNonSecureDistributor[] = "sxg.non_secure_distributor";
constexpr char kSXGResultParseError[] = "sxg.parse_error";
constexpr char kSXGResultInvalidIntegrityHeader[] =
    "sxg.invalid_integrity_header";
constexpr char kSXGResultSignatureVerificationError[] =
    "sxg.signature_verification_error";
constexpr char kSXGResultCertVerificationError[] =
    "sxg.cert_verification_error";
constexpr char kSXGResultCertFetchError[] = "sxg.cert_fetch_error";
constexpr char kSXGResultCertParseError[] = "sxg.cert_parse_error";
constexpr char kSXGResultVariantMismatch[] = "sxg.variant_mismatch";
constexpr char kSXGHeaderIntegrityMismatch[] = "sxg.header_integrity_mismatch";
constexpr char kSXGResultHadCookie[] = "sxg.had_cookie";

const char* GetResultTypeString(SignedExchangeLoadResult result) {
  switch (result) {
    case SignedExchangeLoadResult::kSuccess:
      return kSXGResultOk;
    case SignedExchangeLoadResult::kSXGServedFromNonHTTPS:
      return kSXGResultNonSecureDistributor;
    case SignedExchangeLoadResult::kFallbackURLParseError:
    case SignedExchangeLoadResult::kVersionMismatch:
    case SignedExchangeLoadResult::kHeaderParseError:
    case SignedExchangeLoadResult::kSXGHeaderNetError:
      return kSXGResultParseError;
    case SignedExchangeLoadResult::kCertFetchError:
      return kSXGResultCertFetchError;
    case SignedExchangeLoadResult::kCertParseError:
      return kSXGResultCertParseError;
    case SignedExchangeLoadResult::kSignatureVerificationError:
      return kSXGResultSignatureVerificationError;
    case SignedExchangeLoadResult::kCertVerificationError:
      return kSXGResultCertVerificationError;
    case SignedExchangeLoadResult::kCTVerificationError:
      return kSXGResultCertVerificationError;
    case SignedExchangeLoadResult::kOCSPError:
      return kSXGResultCertVerificationError;
    case SignedExchangeLoadResult::kCertRequirementsNotMet:
    case SignedExchangeLoadResult::kCertValidityPeriodTooLong:
      return kSXGResultCertVerificationError;
    case SignedExchangeLoadResult::kMerkleIntegrityError:
      return kSXGResultMiError;
    case SignedExchangeLoadResult::kSXGServedWithoutNosniff:
      // TODO(crbug.com/40604536): Need to update the spec to send the report in
      // this case.
      return kSXGResultParseError;
    case SignedExchangeLoadResult::kInvalidIntegrityHeader:
      return kSXGResultInvalidIntegrityHeader;
    case SignedExchangeLoadResult::kVariantMismatch:
      // TODO(crbug.com/40604536): Need to update the spec to send the report in
      // this case.
      return kSXGResultVariantMismatch;
    case SignedExchangeLoadResult::kHadCookieForCookielessOnlySXG:
      // TODO(crbug.com/40604536): Need to update the spec to send the report in
      // this case.
      return kSXGResultHadCookie;
    case SignedExchangeLoadResult::kPKPViolationError:
      return kSXGResultCertVerificationError;
  }
  NOTREACHED_IN_MIGRATION();
  return kSXGResultFailed;
}

bool IsCertRelatedErrorResult(const char* result_string) {
  return result_string == kSXGResultSignatureVerificationError ||
         result_string == kSXGResultCertVerificationError ||
         result_string == kSXGResultCertFetchError ||
         result_string == kSXGResultCertParseError;
}

// [spec text]
// - If report body’s "sxg"'s "cert_url"'s scheme is not "data" and report’s
//   result is "signature_verification_error" or "cert_verification_error" or
//   "cert_fetch_error" or "cert_parse_error":
//   - If report’s outer request's url's origin is different from any origin
//     of the URLs in report’s cert URL list, or report’s server IP is
//     different from any of the IP address in report’s cert server IP list:
bool ShouldDowngradeReport(const char* result_string,
                           const network::mojom::SignedExchangeReport& report,
                           const net::IPAddress& cert_server_ip_address) {
  if (report.cert_url.SchemeIs("data"))
    return false;
  if (!IsCertRelatedErrorResult(result_string))
    return false;
  if (url::Origin::Create(report.outer_url) !=
      url::Origin::Create(report.cert_url)) {
    return true;
  }
  if (!cert_server_ip_address.empty() &&
      cert_server_ip_address != report.server_ip_address) {
    return true;
  }
  return false;
}

void ReportResult(
    FrameTreeNodeId frame_tree_node_id,
    network::mojom::SignedExchangeReportPtr report,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  FrameTreeNode* frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id);
  if (!frame_tree_node)
    return;
  RenderFrameHostImpl* frame_host = frame_tree_node->current_frame_host();
  if (!frame_host)
    return;
  SiteInstance* site_instance = frame_host->GetSiteInstance();
  DCHECK(site_instance);
  StoragePartition* partition =
      frame_host->GetBrowserContext()->GetStoragePartition(site_instance);
  DCHECK(partition);
  partition->GetNetworkContext()->QueueSignedExchangeReport(
      std::move(report), network_anonymization_key);
}

}  // namespace

// static
std::unique_ptr<SignedExchangeReporter> SignedExchangeReporter::MaybeCreate(
    const GURL& outer_url,
    const std::string& referrer,
    const network::mojom::URLResponseHead& response,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    FrameTreeNodeId frame_tree_node_id) {
  if (!signed_exchange_utils::
          IsSignedExchangeReportingForDistributorsEnabled()) {
    return nullptr;
  }
  return base::WrapUnique(new SignedExchangeReporter(
      outer_url, referrer, response, network_anonymization_key,
      frame_tree_node_id));
}

SignedExchangeReporter::SignedExchangeReporter(
    const GURL& outer_url,
    const std::string& referrer,
    const network::mojom::URLResponseHead& response,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    FrameTreeNodeId frame_tree_node_id)
    : report_(network::mojom::SignedExchangeReport::New()),
      request_start_(response.load_timing.request_start),
      network_anonymization_key_(network_anonymization_key),
      frame_tree_node_id_(frame_tree_node_id) {
  report_->outer_url = outer_url;
  report_->referrer = referrer;
  report_->server_ip_address = response.remote_endpoint.address();
  // If we got response headers, assume that the connection used HTTP/1.1
  // unless ALPN negotiation tells us otherwise (handled below).
  report_->protocol = response.was_alpn_negotiated
                          ? response.alpn_negotiated_protocol
                          : std::string("http/1.1");
  report_->status_code =
      response.headers ? response.headers->response_code() : 0;
  report_->method = "GET";
}

SignedExchangeReporter::~SignedExchangeReporter() = default;

void SignedExchangeReporter::set_cert_server_ip_address(
    const net::IPAddress& cert_server_ip_address) {
  cert_server_ip_address_ = cert_server_ip_address;
}

void SignedExchangeReporter::set_inner_url(const GURL& inner_url) {
  DCHECK(report_);
  report_->inner_url = inner_url;
}

void SignedExchangeReporter::set_cert_url(const GURL& cert_url) {
  DCHECK(report_);
  report_->cert_url = cert_url;
}

void SignedExchangeReporter::ReportLoadResultAndFinish(
    SignedExchangeLoadResult result) {
  DCHECK(report_);

  const char* result_string = GetResultTypeString(result);
  report_->success = result == SignedExchangeLoadResult::kSuccess;

  if (ShouldDowngradeReport(result_string, *report_, cert_server_ip_address_)) {
    // If the report should be downgraded (See the comment of
    // ShouldDowngradeReport):
    // [spec text]
    //   - Set report body’s "type" to "sxg.failed".
    //   - Set report body’s "elapsed_time" to 0.
    report_->type = kSXGResultFailed;
    report_->elapsed_time = base::TimeDelta();
  } else {
    report_->type = result_string;
    report_->elapsed_time = base::TimeTicks::Now() - request_start_;
  }

  ReportResult(frame_tree_node_id_, std::move(report_),
               network_anonymization_key_);
}

void SignedExchangeReporter::ReportHeaderIntegrityMismatch() {
  DCHECK(report_);
  report_->success = false;
  report_->type = kSXGHeaderIntegrityMismatch;
  report_->elapsed_time = base::TimeDelta();
  ReportResult(frame_tree_node_id_, std::move(report_),
               network_anonymization_key_);
}

}  // namespace content
