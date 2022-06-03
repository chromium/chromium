// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

WebFeature GetWebFeature(
    SubresourceIntegrity::ReportInfo::UseCounterFeature& feature) {
  switch (feature) {
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRIElementWithMatchingIntegrityAttribute:
      return WebFeature::kSRIElementWithMatchingIntegrityAttribute;
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRIElementWithNonMatchingIntegrityAttribute:
      return WebFeature::kSRIElementWithNonMatchingIntegrityAttribute;
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRIElementIntegrityAttributeButIneligible:
      return WebFeature::kSRIElementIntegrityAttributeButIneligible;
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRIElementWithUnparsableIntegrityAttribute:
      return WebFeature::kSRIElementWithUnparsableIntegrityAttribute;
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRISignatureCheck:
      return WebFeature::kSRISignatureCheck;
    case SubresourceIntegrity::ReportInfo::UseCounterFeature::
        kSRISignatureSuccess:
      return WebFeature::kSRISignatureSuccess;
  }
  NOTREACHED();
  return WebFeature::kSRIElementWithUnparsableIntegrityAttribute;
}

void SubresourceIntegrityHelper::DoReport(
    ExecutionContext& execution_context,
    const SubresourceIntegrity::ReportInfo& report_info) {
  for (auto feature : report_info.UseCounts()) {
    UseCounter::Count(&execution_context, GetWebFeature(feature));
  }
  HeapVector<Member<ConsoleMessage>> messages;
  GetConsoleMessages(report_info, &messages);
  for (const auto& message : messages) {
    execution_context.AddConsoleMessage(message);
  }
}

void SubresourceIntegrityHelper::GetConsoleMessages(
    const SubresourceIntegrity::ReportInfo& report_info,
    HeapVector<Member<ConsoleMessage>>* messages) {
  DCHECK(messages);
  for (const auto& message : report_info.ConsoleErrorMessages()) {
    messages->push_back(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError, message));
  }
}

SubresourceIntegrity::IntegrityFeatures SubresourceIntegrityHelper::GetFeatures(
    ExecutionContext* execution_context) {
  bool allow_signatures =
      RuntimeEnabledFeatures::SignatureBasedIntegrityEnabledByRuntimeFlag() ||
      RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled(execution_context);
  return allow_signatures ? SubresourceIntegrity::IntegrityFeatures::kSignatures
                          : SubresourceIntegrity::IntegrityFeatures::kDefault;
}

}  // namespace blink
