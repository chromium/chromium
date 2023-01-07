// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"

namespace blink {

DocumentPolicyViolationReportBody::DocumentPolicyViolationReportBody(
    const String& feature_id,
    const String& message,
    const String& disposition,
    // URL of the resource that violated the document policy.
    const String& resource_url)
    : LocationReportBody(resource_url),
      feature_id_(feature_id),
      message_("Document policy violation: " +
               (message.empty()
                    ? feature_id + " is not allowed in this document."
                    : message)),
      disposition_(disposition) {
  DCHECK(!feature_id.empty());
  DCHECK(!disposition.empty());
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
  hash = WTF::HashInts(hash, featureId().Impl()->GetHash());
  hash = WTF::HashInts(hash, disposition().Impl()->GetHash());
  hash = WTF::HashInts(hash, message().Impl()->GetHash());
  return hash;
}

}  // namespace blink
