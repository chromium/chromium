// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {

const base::Feature kUkmFeature = {"Ukm", base::FEATURE_ENABLED_BY_DEFAULT};

UkmRecorder::UkmRecorder() = default;

UkmRecorder::~UkmRecorder() = default;

// static
UkmRecorder* UkmRecorder::Get() {
  // Note that SourceUrlRecorderWebContentsObserver assumes that
  // DelegatingUkmRecorder::Get() is the canonical UkmRecorder instance. If this
  // changes, SourceUrlRecorderWebContentsObserver should be updated to match.
  return DelegatingUkmRecorder::Get();
}

// static
ukm::SourceId UkmRecorder::GetNewSourceID() {
  return AssignNewSourceId();
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForPaymentAppFromScope(
    base::PassKey<content::PaymentAppProviderUtil>,
    const GURL& service_worker_scope) {
  return UkmRecorder::GetSourceIdFromScopeImpl(service_worker_scope,
                                               SourceIdType::PAYMENT_APP_ID);
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForWebApkManifestUrl(
    base::PassKey<WebApkUkmRecorder>,
    const GURL& manifest_url) {
  return UkmRecorder::GetSourceIdFromScopeImpl(manifest_url,
                                               SourceIdType::WEBAPK_ID);
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForDesktopWebAppStartUrl(
    base::PassKey<web_app::DesktopWebAppUkmRecorder>,
    const GURL& start_url) {
  return UkmRecorder::GetSourceIdFromScopeImpl(
      start_url, SourceIdType::DESKTOP_WEB_APP_ID);
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForWebIdentityFromScope(
    base::PassKey<content::FedCmMetrics>,
    const GURL& provider_url) {
  return UkmRecorder::GetSourceIdFromScopeImpl(provider_url,
                                               SourceIdType::WEB_IDENTITY_ID);
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForRedirectUrl(
    base::PassKey<DIPSBounceDetector>,
    const GURL& redirect_url) {
  return UkmRecorder::GetSourceIdFromScopeImpl(redirect_url,
                                               SourceIdType::REDIRECT_ID);
}

void UkmRecorder::RecordOtherURL(ukm::SourceIdObj source_id, const GURL& url) {
  UpdateSourceURL(source_id.ToInt64(), url);
}

void UkmRecorder::RecordAppURL(ukm::SourceIdObj source_id,
                               const GURL& url,
                               const AppType app_type) {
  UpdateAppURL(source_id.ToInt64(), url, app_type);
}

// static
ukm::SourceId UkmRecorder::GetSourceIdFromScopeImpl(const GURL& scope_url,
                                                    SourceIdType type) {
  SourceId source_id =
      SourceIdObj::FromOtherId(GetNewSourceID(), type).ToInt64();
  UkmRecorder::Get()->UpdateSourceURL(source_id, scope_url);
  return source_id;
}

}  // namespace ukm
