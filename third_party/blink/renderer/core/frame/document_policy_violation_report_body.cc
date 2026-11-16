// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

String CreateMessage(const String& feature_id, const String& message) {
  return message.empty() ? StrCat({"Document policy violation: ", feature_id,
                                   " is not allowed in this document."})
                         : StrCat({"Document policy violation: ", message});
}

}  // namespace

DocumentPolicyViolationReportBody::DocumentPolicyViolationReportBody(
    const String& feature_id,
    const String& message,
    const String& disposition,
    // URL of the resource that violated the document policy.
    const String& resource_url)
    : LocationReportBody(resource_url, std::nullopt, std::nullopt),
      feature_id_(feature_id),
      message_(CreateMessage(feature_id, message)),
      disposition_(disposition) {
  CHECK(!feature_id.empty());
  CHECK(!disposition.empty());
}

DocumentPolicyViolationReportBody::DocumentPolicyViolationReportBody(
    const String& feature_id,
    const String& message,
    const String& disposition,
    const ExecutionContext* execution_context)
    : LocationReportBody(execution_context),
      feature_id_(feature_id),
      message_(CreateMessage(feature_id, message)),
      disposition_(disposition) {
  CHECK(!feature_id.empty());
  CHECK(!disposition.empty());
}

void DocumentPolicyViolationReportBody::BuildJSONValue(
    V8ObjectBuilder& builder) const {
  LocationReportBody::BuildJSONValue(builder);
  builder.AddString("featureId", featureId());
  builder.AddString("disposition", disposition());
  builder.AddStringOrNull("message", message());
}

unsigned DocumentPolicyViolationReportBody::MatchId() const {
  unsigned hash = LocationReportBody::MatchId();
  hash = HashInts(hash, featureId().Impl()->GetHash());
  hash = HashInts(hash, disposition().Impl()->GetHash());
  hash = HashInts(hash, message().Impl()->GetHash());
  return hash;
}

}  // namespace blink
