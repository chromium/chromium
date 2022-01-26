// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_opener_policy_reporter.h"

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/source_location.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

// Report attribute names (camelCase):
constexpr char kColumnNumber[] = "columnNumber";
constexpr char kDisposition[] = "disposition";
constexpr char kEffectivePolicy[] = "effectivePolicy";
constexpr char kInitialPopupURL[] = "initialPopupURL";
constexpr char kLineNumber[] = "lineNumber";
constexpr char kNextURL[] = "nextResponseURL";
constexpr char kOpeneeURL[] = "openeeURL";
constexpr char kOpenerURL[] = "openerURL";
constexpr char kOtherDocumentURL[] = "otherDocumentURL";
constexpr char kPreviousURL[] = "previousResponseURL";
constexpr char kProperty[] = "property";
constexpr char kReferrer[] = "referrer";
constexpr char kSourceFile[] = "sourceFile";
constexpr char kType[] = "type";

// Report attribute values:
constexpr char kDispositionEnforce[] = "enforce";
constexpr char kDispositionReporting[] = "reporting";
constexpr char kTypeFromResponse[] = "navigation-from-response";
constexpr char kTypeToResponse[] = "navigation-to-response";

std::string ToString(network::mojom::CrossOriginOpenerPolicyValue coop_value) {
  switch (coop_value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
      return "unsafe-none";
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
      return "same-origin";
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
      return "same-origin-allow-popups";
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
      return "same-origin-plus-coep";
    case network::mojom::CrossOriginOpenerPolicyValue::
        kSameOriginAllowPopupsPlusCoep:
      return "same-origin-allow-popups-plus-coep";
  }
}

FrameTreeNode* TopLevelOpener(FrameTreeNode* frame) {
  FrameTreeNode* opener = frame->original_opener();
  return opener ? opener->frame_tree()->root() : nullptr;
}

// Remove sensitive data from URL used in reports.
std::string SanitizedURL(const GURL& url) {
  // Strip username, password and ref fragment from the URL.
  // Keep only the valid http/https ones.
  //
  // Note: This is the exact same operation used in
  // ReportingServiceImpl::QueueReport() for the |url|.
  return url.GetAsReferrer().spec();
}

class Receiver final : public network::mojom::CrossOriginOpenerPolicyReporter {
 public:
  Receiver(content::CrossOriginOpenerPolicyReporter* reporter,
           std::string initial_popup_url)
      : reporter_(reporter), initial_popup_url_(initial_popup_url) {}
  ~Receiver() final = default;
  Receiver(const Receiver&) = delete;
  Receiver& operator=(const Receiver&) = delete;

 private:
  void QueueAccessReport(network::mojom::CoopAccessReportType report_type,
                         const std::string& property,
                         network::mojom::SourceLocationPtr source_location,
                         const std::string& reported_window_url) final {
    reporter_->QueueAccessReport(report_type, property,
                                 std::move(source_location),
                                 reported_window_url, initial_popup_url_);
  }

  // |reporter_| is always valid, because it owns |this|.
  raw_ptr<const content::CrossOriginOpenerPolicyReporter> reporter_;
  const std::string initial_popup_url_;
};

}  // namespace

CrossOriginOpenerPolicyReporter::CrossOriginOpenerPolicyReporter(
    StoragePartition* storage_partition,
    const GURL& context_url,
    const GURL& context_referrer_url,
    const network::CrossOriginOpenerPolicy& coop,
    const base::UnguessableToken& reporting_source,
    const net::NetworkIsolationKey& network_isolation_key)
    : storage_partition_(storage_partition),
      reporting_source_(reporting_source),
      context_url_(context_url),
      context_referrer_url_(SanitizedURL(context_referrer_url)),
      coop_(coop),
      network_isolation_key_(network_isolation_key) {
  DCHECK(!reporting_source_.is_empty());
}

CrossOriginOpenerPolicyReporter::~CrossOriginOpenerPolicyReporter() = default;

network::mojom::CrossOriginOpenerPolicyReporterParamsPtr
CrossOriginOpenerPolicyReporter::CreateReporterParams(
    bool access_from_coop_page,
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node) {
  bool endpoint_defined =
      coop_.report_only_reporting_endpoint || coop_.reporting_endpoint;

  using network::mojom::CoopAccessReportType;
  CoopAccessReportType report_type;
  if (access_from_coop_page) {
    if (accessing_node == TopLevelOpener(accessed_node))
      report_type = CoopAccessReportType::kAccessFromCoopPageToOpenee;
    else if (accessed_node == TopLevelOpener(accessing_node))
      report_type = CoopAccessReportType::kAccessFromCoopPageToOpener;
    else
      report_type = CoopAccessReportType::kAccessFromCoopPageToOther;
  } else {
    if (accessed_node == TopLevelOpener(accessing_node))
      report_type = CoopAccessReportType::kAccessToCoopPageFromOpenee;
    else if (accessing_node == TopLevelOpener(accessed_node))
      report_type = CoopAccessReportType::kAccessToCoopPageFromOpener;
    else
      report_type = CoopAccessReportType::kAccessToCoopPageFromOther;
  }

  RenderFrameHostImpl* accessing_rfh = accessing_node->current_frame_host();
  RenderFrameHostImpl* accessed_rfh = accessed_node->current_frame_host();
  bool same_origin = accessing_rfh->GetLastCommittedOrigin().IsSameOriginWith(
      accessed_rfh->GetLastCommittedOrigin());
  RenderFrameHostImpl* reported_rfh =
      access_from_coop_page ? accessed_rfh : accessing_rfh;
  RenderFrameHostImpl* reporting_rfh =
      access_from_coop_page ? accessing_rfh : accessed_rfh;
  std::string reported_window_url =
      same_origin ? SanitizedURL(reported_rfh->GetLastCommittedURL()) : "";

  // If the COOP window is the opener, and the other window's popup creator is
  // same-origin with the COOP document, the openee' initial popup URL is
  // reported.
  std::string reported_initial_popup_url;
  if (report_type == CoopAccessReportType::kAccessFromCoopPageToOpenee ||
      report_type == CoopAccessReportType::kAccessToCoopPageFromOpenee) {
    if (reporting_rfh->GetLastCommittedOrigin().IsSameOriginWith(
            reported_rfh->frame_tree_node()->popup_creator_origin())) {
      reported_initial_popup_url =
          SanitizedURL(reported_rfh->frame_tree_node()->initial_popup_url());
    }
  }

  // Warning: Do not send cross-origin sensitive data. They will be read from:
  // 1) A potentially compromised renderer (the accessing window).
  // 2) A network server (defined from the reporter).
  mojo::PendingRemote<network::mojom::CrossOriginOpenerPolicyReporter>
      remote_reporter;
  receiver_set_.Add(
      std::make_unique<Receiver>(this, reported_initial_popup_url),
      remote_reporter.InitWithNewPipeAndPassReceiver());

  return network::mojom::CrossOriginOpenerPolicyReporterParams::New(
      report_type, std::move(remote_reporter), endpoint_defined,
      reported_window_url);
}

void CrossOriginOpenerPolicyReporter::QueueNavigationToCOOPReport(
    const GURL& previous_url,
    bool same_origin_with_previous,
    bool is_report_only) {
  const absl::optional<std::string>& endpoint =
      is_report_only ? coop_.report_only_reporting_endpoint
                     : coop_.reporting_endpoint;
  if (!endpoint)
    return;

  base::DictionaryValue body;
  body.SetString(kDisposition,
                 is_report_only ? kDispositionReporting : kDispositionEnforce);
  body.SetString(kPreviousURL,
                 same_origin_with_previous ? SanitizedURL(previous_url) : "");
  body.SetString(kReferrer, context_referrer_url_);
  body.SetString(kType, kTypeToResponse);
  QueueNavigationReport(std::move(body), *endpoint, is_report_only);
}

void CrossOriginOpenerPolicyReporter::QueueNavigationAwayFromCOOPReport(
    const GURL& next_url,
    bool is_current_source,
    bool same_origin_with_next,
    bool is_report_only) {
  const absl::optional<std::string>& endpoint =
      is_report_only ? coop_.report_only_reporting_endpoint
                     : coop_.reporting_endpoint;
  if (!endpoint)
    return;

  std::string sanitized_next_url;
  if (is_current_source || same_origin_with_next)
    sanitized_next_url = SanitizedURL(next_url);
  base::DictionaryValue body;
  body.SetString(kNextURL, sanitized_next_url);
  body.SetString(kType, kTypeFromResponse);
  QueueNavigationReport(std::move(body), *endpoint, is_report_only);
}

void CrossOriginOpenerPolicyReporter::QueueAccessReport(
    network::mojom::CoopAccessReportType report_type,
    const std::string& property,
    network::mojom::SourceLocationPtr source_location,
    const std::string& reported_window_url,
    const std::string& initial_popup_url) const {
  // Cross-Origin-Opener-Policy-Report-Only is not required to provide
  // endpoints.
  if (!coop_.report_only_reporting_endpoint)
    return;

  const std::string& endpoint = coop_.report_only_reporting_endpoint.value();

  DCHECK(base::FeatureList::IsEnabled(
      network::features::kCrossOriginOpenerPolicy));

  base::DictionaryValue body;
  body.SetStringPath(kType, network::CoopAccessReportTypeToString(report_type));
  body.SetStringPath(kDisposition, kDispositionReporting);
  body.SetStringPath(kEffectivePolicy,
                     ToString(coop_.report_only_value));
  body.SetStringPath(kProperty, property);
  if (network::IsAccessFromCoopPage(report_type) &&
      source_location->url != "") {
    body.SetStringPath(kSourceFile, source_location->url);
    body.SetIntPath(kLineNumber, source_location->line);
    body.SetIntPath(kColumnNumber, source_location->column);
  }

  switch (report_type) {
    // Reporter is the openee:
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOpener:
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOpener:
      body.SetStringPath(kOpenerURL, reported_window_url);
      body.SetStringPath(kReferrer, context_referrer_url_);
      break;

    // Reporter is the opener:
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOpenee:
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOpenee:
      body.SetStringPath(kOpeneeURL, reported_window_url);
      body.SetStringPath(kInitialPopupURL, initial_popup_url);
      break;

    // Other:
    case network::mojom::CoopAccessReportType::kAccessFromCoopPageToOther:
    case network::mojom::CoopAccessReportType::kAccessToCoopPageFromOther:
      body.SetStringPath(kOtherDocumentURL, reported_window_url);
      break;
  }

  storage_partition_->GetNetworkContext()->QueueReport(
      "coop", endpoint, context_url_, reporting_source_, network_isolation_key_,
      absl::nullopt, std::move(body));
}

void CrossOriginOpenerPolicyReporter::QueueNavigationReport(
    base::DictionaryValue body,
    const std::string& endpoint,
    bool is_report_only) {
  body.SetString(kDisposition,
                 is_report_only ? kDispositionReporting : kDispositionEnforce);
  body.SetString(
      kEffectivePolicy,
      ToString(is_report_only ? coop_.report_only_value : coop_.value));
  storage_partition_->GetNetworkContext()->QueueReport(
      "coop", endpoint, context_url_, reporting_source_, network_isolation_key_,
      /*user_agent=*/absl::nullopt, std::move(body));
}

}  // namespace content
