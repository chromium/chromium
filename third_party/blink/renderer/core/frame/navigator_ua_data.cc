// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua_data.h"

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ua_data_values.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/dactyloscoper.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

// Record identifiability study metrics for a single field requested by a
// getHighEntropyValues() call if the user is in the study.
void MaybeRecordMetric(bool record_identifiability,
                       const String& hint,
                       const IdentifiableToken token,
                       ExecutionContext* execution_context) {
  if (!record_identifiability) [[likely]] {
    return;
  }
  auto identifiable_surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues,
      IdentifiableToken(hint.Utf8()));
  IdentifiabilityMetricBuilder(execution_context->UkmSourceID())
      .Add(identifiable_surface, token)
      .Record(execution_context->UkmRecorder());
}

void MaybeRecordMetric(bool record_identifiability,
                       const String& hint,
                       const String& value,
                       ExecutionContext* execution_context) {
  MaybeRecordMetric(record_identifiability, hint,
                    IdentifiableToken(value.Utf8()), execution_context);
}

void MaybeRecordMetric(bool record_identifiability,
                       const String& hint,
                       const Vector<String>& strings,
                       ExecutionContext* execution_context) {
  if (!record_identifiability) [[likely]] {
    return;
  }
  IdentifiableTokenBuilder token_builder;
  for (const auto& s : strings) {
    token_builder.AddAtomic(s.Utf8());
  }
  MaybeRecordMetric(record_identifiability, hint, token_builder.GetToken(),
                    execution_context);
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

void NavigatorUAData::AddBrandFullVersion(const String& brand,
                                          const String& version) {
  NavigatorUABrandVersion* dict = NavigatorUABrandVersion::Create();
  dict->setBrand(brand);
  dict->setVersion(version);
  full_version_list_.push_back(dict);
}

void NavigatorUAData::SetBrandVersionList(
    const UserAgentBrandList& brand_version_list) {
  for (const auto& brand_version : brand_version_list) {
    AddBrandVersion(String::FromUTF8(brand_version.brand),
                    String::FromUTF8(brand_version.version));
  }
}

void NavigatorUAData::SetFullVersionList(
    const UserAgentBrandList& full_version_list) {
  for (const auto& brand_version : full_version_list) {
    AddBrandFullVersion(String::FromUTF8(brand_version.brand),
                        String::FromUTF8(brand_version.version));
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

void NavigatorUAData::SetBitness(const String& bitness) {
  bitness_ = bitness;
}

void NavigatorUAData::SetWoW64(bool wow64) {
  is_wow64_ = wow64;
}

void NavigatorUAData::SetFormFactors(Vector<String> form_factors) {
  form_factors_ = std::move(form_factors);
}

bool NavigatorUAData::mobile() const {
  if (GetExecutionContext()) {
    return is_mobile_;
  }
  return false;
}

const HeapVector<Member<NavigatorUABrandVersion>>& NavigatorUAData::brands()
    const {
  constexpr auto identifiable_surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature,
      WebFeature::kNavigatorUAData_Brands);

  ExecutionContext* context = GetExecutionContext();
  if (context) {
    // Record IdentifiabilityStudy metrics if the client is in the study.
    if (IdentifiabilityStudySettings::Get()->ShouldSampleSurface(
            identifiable_surface)) [[unlikely]] {
      IdentifiableTokenBuilder token_builder;
      for (const auto& brand : brand_set_) {
        token_builder.AddValue(brand->hasBrand());
        if (brand->hasBrand())
          token_builder.AddAtomic(brand->brand().Utf8());
        token_builder.AddValue(brand->hasVersion());
        if (brand->hasVersion())
          token_builder.AddAtomic(brand->version().Utf8());
      }
      IdentifiabilityMetricBuilder(context->UkmSourceID())
          .Add(identifiable_surface, token_builder.GetToken())
          .Record(context->UkmRecorder());
    }

    return brand_set_;
  }

  return empty_brand_set_;
}

const String& NavigatorUAData::platform() const {
  if (GetExecutionContext()) {
    return platform_;
  }
  return WTF::g_empty_string;
}

ScriptPromise<UADataValues> NavigatorUAData::getHighEntropyValues(
    ScriptState* script_state,
    Vector<String>& hints) const {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<UADataValues>>(script_state);
  auto promise = resolver->Promise();
  auto* execution_context =
      ExecutionContext::From(script_state);  // GetExecutionContext();
  DCHECK(execution_context);

  bool record_identifiability =
      IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kNavigatorUAData_GetHighEntropyValues);
  UADataValues* values = MakeGarbageCollected<UADataValues>();
  // TODO: It'd be faster to compare hint when turning |hints| into an
  // AtomicString vector and turning the const string literals |hint| into
  // AtomicStrings as well.

  // According to
  // https://wicg.github.io/ua-client-hints/#getHighEntropyValues, brands,
  // mobile and platform should be included regardless of whether they were
  // asked for.

  // Use `brands()` and not `brand_set_` directly since the former also
  // records IdentifiabilityStudy metrics.
  values->setBrands(brands());
  values->setMobile(is_mobile_);
  values->setPlatform(platform_);
  // Record IdentifiabilityStudy metrics for `mobile()` and `platform()` (the
  // `brands()` part is already recorded inside that function).
  Dactyloscoper::RecordDirectSurface(
      GetExecutionContext(), WebFeature::kNavigatorUAData_Mobile, mobile());
  Dactyloscoper::RecordDirectSurface(
      GetExecutionContext(), WebFeature::kNavigatorUAData_Platform, platform());

  for (const String& hint : hints) {
    if (hint == "platformVersion") {
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
    } else if (hint == "bitness") {
      values->setBitness(bitness_);
      MaybeRecordMetric(record_identifiability, hint, bitness_,
                        execution_context);
    } else if (hint == "fullVersionList") {
      values->setFullVersionList(full_version_list_);
    } else if (hint == "wow64") {
      values->setWow64(is_wow64_);
      MaybeRecordMetric(record_identifiability, hint, is_wow64_ ? "?1" : "?0",
                        execution_context);
    } else if (hint == "formFactors") {
      values->setFormFactors(form_factors_);
      MaybeRecordMetric(record_identifiability, hint, form_factors_,
                        execution_context);
    }
  }

  execution_context->GetTaskRunner(TaskType::kPermission)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce([](ScriptPromiseResolver<UADataValues>* resolver,
                           UADataValues* values) { resolver->Resolve(values); },
                        WrapPersistent(resolver), WrapPersistent(values)));

  return promise;
}

ScriptValue NavigatorUAData::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddVector<NavigatorUABrandVersion>("brands", brands());
  builder.AddBoolean("mobile", mobile());
  builder.AddString("platform", platform());

  // Record IdentifiabilityStudy metrics for `mobile()` and `platform()`
  // (the `brands()` part is already recorded inside that function).
  Dactyloscoper::RecordDirectSurface(
      GetExecutionContext(), WebFeature::kNavigatorUAData_Mobile, mobile());
  Dactyloscoper::RecordDirectSurface(
      GetExecutionContext(), WebFeature::kNavigatorUAData_Platform, platform());

  return builder.GetScriptValue();
}

void NavigatorUAData::Trace(Visitor* visitor) const {
  visitor->Trace(brand_set_);
  visitor->Trace(full_version_list_);
  visitor->Trace(empty_brand_set_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
