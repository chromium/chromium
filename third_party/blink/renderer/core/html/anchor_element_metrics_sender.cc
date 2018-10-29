// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"

namespace blink {

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
    sender = new AnchorElementMetricsSender(document);
    ProvideTo(document, sender);
  }
  return sender;
}

// static
bool AnchorElementMetricsSender::HasAnchorElementMetricsSender(
    Document& document) {
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(features::kRecordAnchorMetricsClicked) ||
      base::FeatureList::IsEnabled(features::kRecordAnchorMetricsVisible);
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
    Vector<mojom::blink::AnchorElementMetricsPtr> metrics) {
  if (!AssociateInterface())
    return;

  metrics_host_->ReportAnchorElementMetricsOnLoad(std::move(metrics));
  has_onload_report_sent_ = true;
  anchor_elements_.clear();
}

void AnchorElementMetricsSender::AddAnchorElement(HTMLAnchorElement& element) {
  if (has_onload_report_sent_)
    return;
  anchor_elements_.insert(&element);
}

const HeapHashSet<Member<HTMLAnchorElement>>&
AnchorElementMetricsSender::GetAnchorElements() const {
  return anchor_elements_;
}

void AnchorElementMetricsSender::Trace(blink::Visitor* visitor) {
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

  document->GetFrame()->GetInterfaceProvider().GetInterface(
      mojo::MakeRequest(&metrics_host_));
  return true;
}

AnchorElementMetricsSender::AnchorElementMetricsSender(Document& document)
    : Supplement<Document>(document) {
  DCHECK(!document.ParentDocument());
}

}  // namespace blink
