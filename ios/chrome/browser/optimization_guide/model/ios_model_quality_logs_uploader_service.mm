// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/ios_model_quality_logs_uploader_service.h"

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/metrics/metrics_log.h"
#import "components/metrics/version_utils.h"
#import "components/metrics_services_manager/metrics_services_manager.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/proto/model_quality_service.pb.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSModelQualityLogsUploaderService::IOSModelQualityLogsUploaderService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : ModelQualityLogsUploaderService(url_loader_factory, pref_service) {}

IOSModelQualityLogsUploaderService::~IOSModelQualityLogsUploaderService() =
    default;

bool IOSModelQualityLogsUploaderService::CanUploadLogs(
    const optimization_guide::MqlsFeatureMetadata* metadata) {
  CHECK(metadata);

  // Skip upload if metrics reporting is disabled.
  if (!GetApplicationContext()
           ->GetMetricsServicesManager()
           ->IsMetricsConsentGiven()) {
    return false;
  }

  //  Skip upload if logging is disabled for the feature.
  if (!optimization_guide::features::IsModelQualityLoggingEnabledForFeature(
          metadata)) {
    return false;
  }
  // TODO(crbug.com/448433648): Add Enterprise policy check.
  return true;
}

void IOSModelQualityLogsUploaderService::SetSystemMetadata(
    optimization_guide::proto::LoggingMetadata* logging_metadata) {
  CHECK(logging_metadata);

  // Set system profile proto before uploading. Use the core system profile.
  metrics::MetricsLog::RecordCoreSystemProfile(
      metrics::GetVersionString(), metrics::AsProtobufChannel(::GetChannel()),
      /*is_extended_stable_channel=*/false,
      GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
      /*package_name=*/std::string(),
      logging_metadata->mutable_system_profile());
  // Remove identifiers for privacy reasons.
  logging_metadata->mutable_system_profile()->clear_client_uuid();
  logging_metadata->mutable_system_profile()
      ->mutable_cloned_install_info()
      ->clear_cloned_from_client_id();

  // TODO(crbug.com/448433648): Set `IsLikelyDogfoodClient` if relevant.
}
