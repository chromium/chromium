// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cross_origin_opener_policy_reporter.h"

#include "base/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
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
  }
}

base::Optional<base::UnguessableToken> GetFrameToken(
    FrameTreeNode* frame,
    SiteInstance* site_instance) {
  RenderFrameHostImpl* rfh = frame->current_frame_host();
  if (rfh->GetSiteInstance() == site_instance)
    return rfh->GetFrameToken();

  RenderFrameProxyHost* proxy =
      frame->render_manager()->GetRenderFrameProxyHost(site_instance);
  if (proxy)
    return proxy->GetFrameToken();

  return base::nullopt;
}

// Find all the related windows that might try to access the new document in
// |frame|, but are in a different virtual browsing context group.
std::vector<FrameTreeNode*> CollectOtherWindowForCoopAccess(
    FrameTreeNode* frame) {
  DCHECK(frame->IsMainFrame());
  SiteInstance* site_instance = frame->current_frame_host()->GetSiteInstance();
  int virtual_browsing_context_group =
      frame->current_frame_host()->virtual_browsing_context_group();

  std::vector<FrameTreeNode*> out;
  for (WebContentsImpl* wc : WebContentsImpl::GetAllWebContents()) {
    RenderFrameHostImpl* rfh = wc->GetMainFrame();

    // Filters out windows from a different browsing context group.
    if (!rfh->GetSiteInstance()->IsRelatedSiteInstance(site_instance))
      continue;

    // Filter out windows from the same virtual browsing context group.
    if (rfh->virtual_browsing_context_group() == virtual_browsing_context_group)
      continue;

    out.push_back(rfh->frame_tree_node());
  }
  return out;
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

class Receiver : public network::mojom::CrossOriginOpenerPolicyReporter {
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
  const content::CrossOriginOpenerPolicyReporter* reporter_;
  const std::string initial_popup_url_;
};

}  // namespace

CrossOriginOpenerPolicyReporter::CrossOriginOpenerPolicyReporter(
    StoragePartition* storage_partition,
    const GURL& context_url,
    const GURL& context_referrer_url,
    const network::CrossOriginOpenerPolicy& coop)
    : storage_partition_(storage_partition),
      context_url_(context_url),
      context_referrer_url_(SanitizedURL(context_referrer_url)),
      coop_(coop) {}

CrossOriginOpenerPolicyReporter::~CrossOriginOpenerPolicyReporter() = default;

void CrossOriginOpenerPolicyReporter::QueueNavigationToCOOPReport(
    const GURL& previous_url,
    bool same_origin_with_previous,
    bool is_report_only) {
  const base::Optional<std::string>& endpoint =
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
  const base::Optional<std::string>& endpoint =
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
      network::features::kCrossOriginOpenerPolicyAccessReporting));

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
      "coop", endpoint, context_url_, base::nullopt, std::move(body));
}

// static
void CrossOriginOpenerPolicyReporter::InstallAccessMonitorsIfNeeded(
    FrameTreeNode* frame) {
  if (!frame->IsMainFrame())
    return;

  // The function centralize all the CoopAccessMonitor being added. Checking the
  // flag here ensures the feature to be properly disabled everywhere.
  if (!base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicyAccessReporting)) {
    return;
  }

  // TODO(arthursonzogni): It is too late to update the SiteInstance of the new
  // document. Ideally, this should be split into two parts:
  // - CommitNavigation: Update the new document's SiteInstance.
  // - DidCommitNavigation: Update the other SiteInstances.

  // Find all the related windows that might try to access the new document,
  // but are from a different virtual browsing context group.
  std::vector<FrameTreeNode*> other_main_frames =
      CollectOtherWindowForCoopAccess(frame);

  CrossOriginOpenerPolicyReporter* reporter_frame =
      frame->current_frame_host()->coop_reporter();

  for (FrameTreeNode* other : other_main_frames) {
    CrossOriginOpenerPolicyReporter* reporter_other =
        other->current_frame_host()->coop_reporter();

    // If the current frame has a reporter, install the access monitors to
    // monitor the accesses between this frame and the other frame.
    if (reporter_frame) {
      reporter_frame->MonitorAccesses(frame, other);
      reporter_frame->MonitorAccesses(other, frame);
    }

    // If the other frame has a reporter, install the access monitors to monitor
    // the accesses between this frame and the other frame.
    if (reporter_other) {
      reporter_other->MonitorAccesses(frame, other);
      reporter_other->MonitorAccesses(other, frame);
    }
  }
}

void CrossOriginOpenerPolicyReporter::MonitorAccesses(
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node) {
  DCHECK_NE(accessing_node, accessed_node);
  DCHECK(accessing_node->current_frame_host()->coop_reporter() == this ||
         accessed_node->current_frame_host()->coop_reporter() == this);

  // TODO(arthursonzogni): DCHECK same browsing context group.
  // TODO(arthursonzogni): DCHECK different virtual browsing context group.

  // Accesses are made either from the main frame or its same-origin iframes.
  // Accesses from the cross-origin ones aren't reported.
  //
  // It means all the accessed from the first window are made from documents
  // inside the same SiteInstance. Only one SiteInstance has to be updated.

  RenderFrameHostImpl* accessing_rfh = accessing_node->current_frame_host();
  RenderFrameHostImpl* accessed_rfh = accessed_node->current_frame_host();
  SiteInstance* site_instance = accessing_rfh->GetSiteInstance();

  base::Optional<base::UnguessableToken> accessed_window_token =
      GetFrameToken(accessed_node, site_instance);
  if (!accessed_window_token)
    return;

  bool access_from_coop_page =
      this == accessing_node->current_frame_host()->coop_reporter();

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

  bool same_origin = accessing_rfh->GetLastCommittedOrigin().IsSameOriginWith(
      accessed_rfh->GetLastCommittedOrigin());
  RenderFrameHostImpl* reported_rfh =
      access_from_coop_page ? accessed_rfh : accessing_rfh;
  RenderFrameHostImpl* reporting_rfh =
      access_from_coop_page ? accessing_rfh : accessed_rfh;
  std::string reported_window_url =
      same_origin ? SanitizedURL(reported_rfh->GetLastCommittedURL()) : "";

  bool endpoint_defined =
      coop_.report_only_reporting_endpoint || coop_.reporting_endpoint;

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

  mojo::PendingRemote<network::mojom::CrossOriginOpenerPolicyReporter>
      remote_reporter;
  receiver_set_.Add(
      std::make_unique<Receiver>(this, reported_initial_popup_url),
      remote_reporter.InitWithNewPipeAndPassReceiver());

  // Warning: Do not send cross-origin sensitive data. They will be read from:
  // 1) A potentially compromised renderer (the accessing window).
  // 2) A network server (defined from the reporter).
  accessing_rfh->GetAssociatedLocalMainFrame()->InstallCoopAccessMonitor(
      report_type, *accessed_window_token, std::move(remote_reporter),
      endpoint_defined, std::move(reported_window_url));
}

// static
int CrossOriginOpenerPolicyReporter::NextVirtualBrowsingContextGroup() {
  static int id = -1;
  return ++id;
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
      "coop", endpoint, context_url_, /*user_agent=*/base::nullopt,
      std::move(body));
}

}  // namespace content
