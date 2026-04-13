// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/fake_optimization_guide_service.h"

#import "base/functional/callback.h"
#import "base/strings/strcat.h"
#import "base/task/single_thread_task_runner.h"
#import "components/optimization_guide/proto/common_types.pb.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

FakeOptimizationGuideService::FakeOptimizationGuideService(
    leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
    const base::FilePath& profile_path,
    bool off_the_record,
    const std::string& application_locale,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    PrefService* pref_service,
    BrowserList* browser_list,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : OptimizationGuideService(proto_db_provider,
                               profile_path,
                               off_the_record,
                               application_locale,
                               hint_store,
                               pref_service,
                               browser_list,
                               url_loader_factory,
                               identity_manager) {}

FakeOptimizationGuideService::~FakeOptimizationGuideService() = default;

void FakeOptimizationGuideService::ExecuteModel(
    optimization_guide::ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata,
    const optimization_guide::ModelExecutionOptions& options,
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  // Check if an error was explicitly set for this feature.
  auto error_it = errors_.find(feature);
  if (error_it != errors_.end()) {
    optimization_guide::OptimizationGuideModelExecutionError error =
        optimization_guide::OptimizationGuideModelExecutionError::
            FromModelExecutionError(
                static_cast<
                    optimization_guide::OptimizationGuideModelExecutionError::
                        ModelExecutionError>(error_it->second));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::unexpected(error), nullptr),
            nullptr));
    return;
  }

  // Check if a response was explicitly set for this feature.
  auto response_it = responses_.find(feature);
  if (response_it != responses_.end()) {
    optimization_guide::proto::Any any;
    if (any.ParseFromString(response_it->second)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              optimization_guide::OptimizationGuideModelExecutionResult(
                  base::ok(any), nullptr),
              nullptr));
      return;
    }
  }

  // Default failure if no response or error is set.
  optimization_guide::OptimizationGuideModelExecutionError error =
      optimization_guide::OptimizationGuideModelExecutionError::
          FromModelExecutionError(
              optimization_guide::OptimizationGuideModelExecutionError::
                  ModelExecutionError::kGenericFailure);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     optimization_guide::OptimizationGuideModelExecutionResult(
                         base::unexpected(error), nullptr),
                     nullptr));
}

void FakeOptimizationGuideService::SetResponse(
    optimization_guide::ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& response,
    const std::string& type_name) {
  // Wrap the response in an `Any` proto, as the real service returns results
  // wrapped in an `Any` object.
  optimization_guide::proto::Any any;
  any.set_type_url(base::StrCat({"type.googleapis.com/", type_name}));
  any.set_value(response.SerializeAsString());
  responses_[feature] = any.SerializeAsString();
}

void FakeOptimizationGuideService::SetError(
    optimization_guide::ModelBasedCapabilityKey feature,
    int error_code) {
  errors_[feature] = error_code;
}
