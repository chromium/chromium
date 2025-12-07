// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/report_body.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_coop_access_violation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_csp_violation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_deprecation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_policy_violation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_integrity_violation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intervention_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_permissions_policy_violation_report_body.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_report_body.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/coop_access_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/integrity_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/intervention_report_body.h"
#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/test_report_body.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

WebFeature GetFeatureForReportBodyType(const ReportBody* body) {
  const WrapperTypeInfo* type_info = body->GetWrapperTypeInfo();

  if (type_info == V8CSPViolationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kCSPViolationReportBodyToJSON;
  }
  if (type_info == V8DeprecationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kDeprecationReportBodyToJSON;
  }
  if (type_info == V8InterventionReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kInterventionReportBodyToJSON;
  }
  if (type_info == V8DocumentPolicyViolationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kDocumentPolicyViolationReportBodyToJSON;
  }
  if (type_info == V8IntegrityViolationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kIntegrityViolationReportBodyToJSON;
  }
  if (type_info ==
      V8PermissionsPolicyViolationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kPermissionsPolicyViolationReportBodyToJSON;
  }
  if (type_info == V8CoopAccessViolationReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kCoopAccessViolationReportBodyToJSON;
  }
  if (type_info == V8TestReportBody::GetWrapperTypeInfo()) {
    return WebFeature::kTestReportBodyToJSON;
  }
  // Base ReportBody type
  return WebFeature::kReportBodyToJSON;
}

}  // namespace

// This overload is called when toJSON() is invoked explicitly without
// arguments. We count this case to measure the potential impact of migrating
// ReportBody to a dictionary type, which will remove the explicit toJSON()
// method.
ScriptObject ReportBody::toJSON(ScriptState* script_state) const {
  WebFeature feature = GetFeatureForReportBodyType(this);
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (context) {
    UseCounter::Count(context, feature);
  }

  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.ToScriptObject();
}

// When `key` is provided, this indicates that `toJSON()` was being
// called implicitly by `JSON.stringify()`. (While it's technically
// possible for a developer to pass in `key` explicitly and trick the
// code here, we expect that to be rare for the purposes of our use counter.)
ScriptObject ReportBody::toJSON(ScriptState* script_state,
                                const String& key) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.ToScriptObject();
}

}  // namespace blink
