// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/report.h"

namespace blink {

constexpr const char ReportType::kCSPViolation[];
constexpr const char ReportType::kCoopAccessViolation[];
constexpr const char ReportType::kDeprecation[];
constexpr const char ReportType::kDocumentPolicyViolation[];
constexpr const char ReportType::kPermissionsPolicyViolation[];
constexpr const char ReportType::kIntervention[];

ScriptValue Report::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("type", type());
  builder.AddString("url", url());
  V8ObjectBuilder body_builder(script_state);
  body()->BuildJSONValue(body_builder);
  builder.Add("body", body_builder);
  return builder.GetScriptValue();
}

unsigned Report::MatchId() const {
  unsigned hash = body()->MatchId();
  hash = WTF::HashInts(hash, url().IsNull() ? 0 : url().Impl()->GetHash());
  hash = WTF::HashInts(hash, type().Impl()->GetHash());
  return hash;
}

bool Report::ShouldSendReport() const {
  // Don't report any URLs from extension code.
  // TODO(356098278): Investigate whether extension URLs should be reported to
  // an extension-defined endpoint, if the extension opts in to reporting.
  return !body()->IsExtensionSource();
}

}  // namespace blink
