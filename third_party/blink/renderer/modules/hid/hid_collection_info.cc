// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_collection_info.h"

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "third_party/blink/renderer/modules/hid/hid_report_info.h"

namespace blink {

HIDCollectionInfo::HIDCollectionInfo(
    const device::mojom::blink::HidCollectionInfo& info)
    : usage_page_(info.usage->usage_page),
      usage_(info.usage->usage),
      collection_type_(info.collection_type) {
  for (const auto& child : info.children)
    children_.push_back(MakeGarbageCollected<HIDCollectionInfo>(*child));
  for (const auto& report : info.input_reports)
    input_reports_.push_back(MakeGarbageCollected<HIDReportInfo>(*report));
  for (const auto& report : info.output_reports)
    output_reports_.push_back(MakeGarbageCollected<HIDReportInfo>(*report));
  for (const auto& report : info.feature_reports)
    feature_reports_.push_back(MakeGarbageCollected<HIDReportInfo>(*report));
}

HIDCollectionInfo::~HIDCollectionInfo() = default;

uint16_t HIDCollectionInfo::usagePage() const {
  return usage_page_;
}

uint16_t HIDCollectionInfo::usage() const {
  return usage_;
}

const HeapVector<Member<HIDCollectionInfo>>& HIDCollectionInfo::children()
    const {
  return children_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::inputReports()
    const {
  return input_reports_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::outputReports()
    const {
  return output_reports_;
}

const HeapVector<Member<HIDReportInfo>>& HIDCollectionInfo::featureReports()
    const {
  return feature_reports_;
}

uint32_t HIDCollectionInfo::collectionType() const {
  return collection_type_;
}

void HIDCollectionInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(children_);
  visitor->Trace(input_reports_);
  visitor->Trace(output_reports_);
  visitor->Trace(feature_reports_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
