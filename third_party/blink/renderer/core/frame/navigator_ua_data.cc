// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua_data.h"

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ua_data_values.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature_forward.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

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
  if (GetExecutionContext()) {
    return brand_set_;
  }

  return empty_brand_set_;
}

const String& NavigatorUAData::platform() const {
  if (GetExecutionContext()) {
    return platform_;
  }
  return g_empty_string;
}

bool AllowedToCollectHighEntropyValues(ExecutionContext* execution_context) {
  // To determine whether a document is allowed to use the get high-entropy
  // client hints returned by navigator.userAgentData.getHighEntropyValues(),
  // check the following:

  // 1. Check if our RuntimeEnabledFeature is enabled
  // Note: We return true if not enabled because the default allowlist is "*",
  // this permissions-policy allows a document to restrict it.
  // TODO(crbug.com/388538952): remove this after it ships to stable
  if (!RuntimeEnabledFeatures::
          ClientHintUAHighEntropyValuesPermissionPolicyEnabled()) {
    return true;
  }

  // 2. If Permissions Policy is enabled, return the policy for
  // "ch-ua-high-entropy-values" feature.
  return execution_context->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kClientHintUAHighEntropyValues,
      ReportOptions::kReportOnFailure,
      "Collection of high-entropy user-agent client hints is disabled for "
      "this document.");
}

ScriptPromise<UADataValues> NavigatorUAData::getHighEntropyValues(
    ScriptState* script_state,
    const Vector<String>& hints) const {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<UADataValues>>(script_state);
  auto promise = resolver->Promise();
  auto* execution_context =
      ExecutionContext::From(script_state);  // GetExecutionContext();
  DCHECK(execution_context);

  UADataValues* values = MakeGarbageCollected<UADataValues>();
  // TODO: It'd be faster to compare hint when turning |hints| into an
  // AtomicString vector and turning the const string literals |hint| into
  // AtomicStrings as well.

  // According to
  // https://wicg.github.io/ua-client-hints/#getHighEntropyValues, the
  // low-entropy brands, mobile and platform hints should always be included for
  // convenience.

  values->setBrands(brand_set_);
  values->setMobile(is_mobile_);
  values->setPlatform(platform_);

  // If the "ch-ua-high-entropy-values" permission policy is enabled for a
  // document, add high-entropy client hints to values (if requested)
  if (AllowedToCollectHighEntropyValues(execution_context)) {
    for (const String& hint : hints) {
      if (hint == "platformVersion") {
        values->setPlatformVersion(platform_version_);
      } else if (hint == "architecture") {
        values->setArchitecture(architecture_);
      } else if (hint == "model") {
        values->setModel(model_);
      } else if (hint == "uaFullVersion") {
        values->setUaFullVersion(ua_full_version_);
      } else if (hint == "bitness") {
        values->setBitness(bitness_);
      } else if (hint == "fullVersionList") {
        values->setFullVersionList(full_version_list_);
      } else if (hint == "wow64") {
        values->setWow64(is_wow64_);
      } else if (hint == "formFactors") {
        values->setFormFactors(form_factors_);
      }
    }
  }

  execution_context->GetTaskRunner(TaskType::kPermission)
      ->PostTask(
          FROM_HERE,
          BindOnce([](ScriptPromiseResolver<UADataValues>* resolver,
                      UADataValues* values) { resolver->Resolve(values); },
                   WrapPersistent(resolver), WrapPersistent(values)));

  return promise;
}

ScriptObject NavigatorUAData::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddVector<NavigatorUABrandVersion>("brands", brands());
  builder.AddBoolean("mobile", mobile());
  builder.AddString("platform", platform());

  return builder.ToScriptObject();
}

void NavigatorUAData::Trace(Visitor* visitor) const {
  visitor->Trace(brand_set_);
  visitor->Trace(full_version_list_);
  visitor->Trace(empty_brand_set_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
