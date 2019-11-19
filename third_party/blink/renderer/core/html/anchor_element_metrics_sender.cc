// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"

namespace blink {

namespace {

// Returns true if |anchor_element| should be discarded, and not used for
// navigation prediction.
bool ShouldDiscardAnchorElement(const HTMLAnchorElement& anchor_element) {
  Frame* frame = anchor_element.GetDocument().GetFrame();
  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame)
    return true;
  return local_frame->IsAdSubframe();
}

}  // namespace

// static
const char AnchorElementMetricsSender::kSupplementName[] =
    "DocumentAnchorElementMetricsSender";

AnchorElementMetricsSender::~AnchorElementMetricsSender() = default;

// static
AnchorElementMetricsSender* AnchorElementMetricsSender::From(
    Document& document) {
  DCHECK(HasAnchorElementMetricsSender(document));

  AnchorElementMetricsSender* sender =
      Supplement<Document>::From<AnchorElementMetricsSender>(document);
  if (!sender) {
    sender = MakeGarbageCollected<AnchorElementMetricsSender>(document);
    ProvideTo(document, sender);
  }
  return sender;
}

// static
bool AnchorElementMetricsSender::HasAnchorElementMetricsSender(
    Document& document) {
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(features::kNavigationPredictor);
  const KURL& url = document.BaseURL();
  return is_feature_enabled && !document.ParentDocument() && url.IsValid() &&
         url.ProtocolIsInHTTPFamily();
}

void AnchorElementMetricsSender::SendClickedAnchorMetricsToBrowser(
    mojom::blink::AnchorElementMetricsPtr metric) {
  if (!AssociateInterface())
    return;

  metrics_host_->ReportAnchorElementMetricsOnClick(std::move(metric));
}

void AnchorElementMetricsSender::SendAnchorMetricsVectorToBrowser(
    Vector<mojom::blink::AnchorElementMetricsPtr> metrics,
    const IntSize& viewport_size) {
  if (!AssociateInterface())
    return;

  metrics_host_->ReportAnchorElementMetricsOnLoad(std::move(metrics),
                                                  viewport_size);
  has_onload_report_sent_ = true;
  anchor_elements_.clear();
}

void AnchorElementMetricsSender::AddAnchorElement(HTMLAnchorElement& element) {
  if (has_onload_report_sent_)
    return;

  bool is_ad_frame_element = ShouldDiscardAnchorElement(element);
  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.IsAdFrameElement",
                        is_ad_frame_element);

  // We ignore anchor elements that are in ad frames.
  if (is_ad_frame_element)
    return;

  anchor_elements_.insert(&element);
}

const HeapHashSet<Member<HTMLAnchorElement>>&
AnchorElementMetricsSender::GetAnchorElements() const {
  return anchor_elements_;
}

void AnchorElementMetricsSender::Trace(Visitor* visitor) {
  visitor->Trace(anchor_elements_);
  Supplement<Document>::Trace(visitor);
}

bool AnchorElementMetricsSender::AssociateInterface() {
  if (metrics_host_)
    return true;

  Document* document = GetSupplementable();
  // Unable to associate since no frame is attached.
  if (!document->GetFrame())
    return false;

  document->GetBrowserInterfaceBroker().GetInterface(
      metrics_host_.BindNewPipeAndPassReceiver());
  return true;
}

AnchorElementMetricsSender::AnchorElementMetricsSender(Document& document)
    : Supplement<Document>(document) {
  DCHECK(!document.ParentDocument());
}

void AnchorElementMetricsSender::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  // Check that layout is stable. If it is, we can perform the onload update and
  // stop observing future events.
  Document* document = local_frame_view.GetFrame().GetDocument();
  if (document->Lifecycle().GetState() <
      DocumentLifecycle::kAfterPerformLayout) {
    return;
  }

  // Stop listening to updates, as the onload report can be sent now.
  document->View()->UnregisterFromLifecycleNotifications(this);

  // Send onload report.
  AnchorElementMetrics::MaybeReportViewportMetricsOnLoad(*document);
}

}  // namespace blink
