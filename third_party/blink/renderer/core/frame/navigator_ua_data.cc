// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua_data.h"

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ua_data_values.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

// Record identifiability study metrics for a single field requested by a
// getHighEntropyValues() call if the user is in the study.
void MaybeRecordMetric(bool record_identifiability,
                       const String& hint,
                       const String& value,
                       ExecutionContext* execution_context) {
  if (LIKELY(!record_identifiability))
    return;
  auto identifiable_surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues,
      IdentifiableToken(hint.Utf8()));
  IdentifiabilityMetricBuilder(execution_context->UkmSourceID())
      .Set(identifiable_surface, IdentifiableToken(value.Utf8()))
      .Record(execution_context->UkmRecorder());
}

}  // namespace

NavigatorUAData::NavigatorUAData(ExecutionContext* context)
    : ExecutionContextClient(context) {
  NavigatorUABrandVersion* dict = NavigatorUABrandVersion::Create();
  dict->setBrand("");
  dict->setVersion("");
  empty_brand_set_.push_back(dict);
}

void NavigatorUAData::AddBrandVersion(const String& brand,
                                      const String& version) {
  NavigatorUABrandVersion* dict = NavigatorUABrandVersion::Create();
  dict->setBrand(brand);
  dict->setVersion(version);
  brand_set_.push_back(dict);
}

void NavigatorUAData::SetBrandVersionList(
    const UserAgentBrandList& brand_version_list) {
  for (const auto& brand_version : brand_version_list) {
    AddBrandVersion(String::FromUTF8(brand_version.brand),
                    String::FromUTF8(brand_version.major_version));
  }
}

void NavigatorUAData::SetMobile(bool mobile) {
  is_mobile_ = mobile;
}

void NavigatorUAData::SetPlatform(const String& brand, const String& version) {
  platform_ = brand;
  platform_version_ = version;
}

void NavigatorUAData::SetArchitecture(const String& architecture) {
  architecture_ = architecture;
}

void NavigatorUAData::SetModel(const String& model) {
  model_ = model;
}

void NavigatorUAData::SetUAFullVersion(const String& ua_full_version) {
  ua_full_version_ = ua_full_version;
}

bool NavigatorUAData::mobile() const {
  if (GetExecutionContext()) {
    return is_mobile_;
  }
  return false;
}

const HeapVector<Member<NavigatorUABrandVersion>>& NavigatorUAData::brands()
    const {
  if (GetExecutionContext()) {
    return brand_set_;
  }
  return empty_brand_set_;
}

ScriptPromise NavigatorUAData::getHighEntropyValues(
    ScriptState* script_state,
    Vector<String>& hints) const {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  auto* execution_context =
      ExecutionContext::From(script_state);  // GetExecutionContext();
  DCHECK(execution_context);

  bool record_identifiability =
      IdentifiabilityStudySettings::Get()->ShouldSample(
          IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues);
  UADataValues* values = MakeGarbageCollected<UADataValues>();
  for (const String& hint : hints) {
    if (hint == "platform") {
      values->setPlatform(platform_);
      MaybeRecordMetric(record_identifiability, hint, platform_,
                        execution_context);
    } else if (hint == "platformVersion") {
      values->setPlatformVersion(platform_version_);
      MaybeRecordMetric(record_identifiability, hint, platform_version_,
                        execution_context);
    } else if (hint == "architecture") {
      values->setArchitecture(architecture_);
      MaybeRecordMetric(record_identifiability, hint, architecture_,
                        execution_context);
    } else if (hint == "model") {
      values->setModel(model_);
      MaybeRecordMetric(record_identifiability, hint, model_,
                        execution_context);
    } else if (hint == "uaFullVersion") {
      values->setUaFullVersion(ua_full_version_);
      MaybeRecordMetric(record_identifiability, hint, ua_full_version_,
                        execution_context);
    }
  }

  execution_context->GetTaskRunner(TaskType::kPermission)
      ->PostTask(
          FROM_HERE,
          WTF::Bind([](ScriptPromiseResolver* resolver,
                       UADataValues* values) { resolver->Resolve(values); },
                    WrapPersistent(resolver), WrapPersistent(values)));

  return promise;
}

void NavigatorUAData::Trace(Visitor* visitor) const {
  visitor->Trace(brand_set_);
  visitor->Trace(empty_brand_set_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
