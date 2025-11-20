// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// iOS-specific implementation of ModelQualityLogsUploaderService.
class IOSModelQualityLogsUploaderService
    : public optimization_guide::ModelQualityLogsUploaderService {
 public:
  IOSModelQualityLogsUploaderService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);

  IOSModelQualityLogsUploaderService(
      const IOSModelQualityLogsUploaderService&) = delete;
  IOSModelQualityLogsUploaderService& operator=(
      const IOSModelQualityLogsUploaderService&) = delete;

  ~IOSModelQualityLogsUploaderService() override;

  // optimization_guide::ModelQualityLogsUploaderService:
  bool CanUploadLogs(
      const optimization_guide::MqlsFeatureMetadata* metadata) override;
  void SetSystemMetadata(
      optimization_guide::proto::LoggingMetadata* logging_metadata) override;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
