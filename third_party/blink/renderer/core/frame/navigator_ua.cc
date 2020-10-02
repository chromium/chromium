// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

NavigatorUAData* NavigatorUA::userAgentData() {
  NavigatorUAData* ua_data =
      MakeGarbageCollected<NavigatorUAData>(GetUAExecutionContext());

  UserAgentMetadata metadata = GetUserAgentMetadata();
  ua_data->SetBrandVersionList(metadata.brand_version_list);
  ua_data->SetMobile(metadata.mobile);
  ua_data->SetPlatform(String::FromUTF8(metadata.platform),
                       String::FromUTF8(metadata.platform_version));
  ua_data->SetArchitecture(String::FromUTF8(metadata.architecture));
  ua_data->SetModel(String::FromUTF8(metadata.model));
  ua_data->SetUAFullVersion(String::FromUTF8(metadata.full_version));

  MaybeRecordMetrics(*ua_data);

  return ua_data;
}

// Records identifiability study metrics if the user is in the study.
void NavigatorUA::MaybeRecordMetrics(const NavigatorUAData& ua_data) {
  constexpr auto identifiable_surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, WebFeature::kNavigatorUserAgent);
  if (LIKELY(!IdentifiabilityStudySettings::Get()->IsSurfaceAllowed(
          identifiable_surface))) {
    return;
  }

  // Only instrument low-entropy fields here. The other fields are
  // instrumented separately in NavigatorUAData::getHighEntropyValues().
  IdentifiableTokenBuilder token_builder;
  token_builder.AddValue(ua_data.mobile());
  for (const auto& brand : ua_data.brands()) {
    token_builder.AddValue(brand->hasBrand());
    if (brand->hasBrand())
      token_builder.AddAtomic(brand->brand().Utf8());
    token_builder.AddValue(brand->hasVersion());
    if (brand->hasVersion())
      token_builder.AddAtomic(brand->version().Utf8());
  }
  ExecutionContext* context = GetUAExecutionContext();
  IdentifiabilityMetricBuilder(context->UkmSourceID())
      .Set(identifiable_surface, token_builder.GetToken())
      .Record(context->UkmRecorder());
}

}  // namespace blink
