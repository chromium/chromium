// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class ModelExecutionFeaturesController;
class MqlsFeatureMetadata;
}  // namespace optimization_guide

// iOS-specific implementation of ModelQualityLogsUploaderService.
class IOSModelQualityLogsUploaderService
    : public optimization_guide::ModelQualityLogsUploaderService {
 public:
  IOSModelQualityLogsUploaderService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      base::WeakPtr<optimization_guide::ModelExecutionFeaturesController>
          model_execution_feature_controller);

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

 private:
  // This allows checking for enterprise policy on upload.
  base::WeakPtr<optimization_guide::ModelExecutionFeaturesController>
      model_execution_feature_controller_;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
