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
    const GURL& service_worker_scope) {
  ukm::SourceId source_id = ukm::SourceIdObj::FromOtherId(
                                GetNewSourceID(), SourceIdType::PAYMENT_APP_ID)
                                .ToInt64();
  ukm::UkmRecorder::Get()->UpdateSourceURL(source_id, service_worker_scope);
  return source_id;
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForWebApkManifestUrl(
    const GURL& manifest_url) {
  ukm::SourceId source_id =
      ukm::SourceIdObj::FromOtherId(GetNewSourceID(), SourceIdType::WEBAPK_ID)
          .ToInt64();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);
  return source_id;
}

// static
ukm::SourceId UkmRecorder::GetSourceIdForDesktopWebAppStartUrl(
    const GURL& start_url) {
  ukm::SourceId source_id =
      ukm::SourceIdObj::FromOtherId(GetNewSourceID(),
                                    SourceIdType::DESKTOP_WEB_APP_ID)
          .ToInt64();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, start_url);
  return source_id;
}

void UkmRecorder::RecordOtherURL(ukm::SourceIdObj source_id, const GURL& url) {
  UpdateSourceURL(source_id.ToInt64(), url);
}

void UkmRecorder::RecordAppURL(ukm::SourceIdObj source_id,
                               const GURL& url,
                               const AppType app_type) {
  UpdateAppURL(source_id.ToInt64(), url, app_type);
}

}  // namespace ukm
