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

}  // namespace blink
