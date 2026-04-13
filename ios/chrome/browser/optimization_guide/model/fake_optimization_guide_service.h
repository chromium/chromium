// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_FAKE_OPTIMIZATION_GUIDE_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_FAKE_OPTIMIZATION_GUIDE_SERVICE_H_

#import <map>
#import <string>

#import "components/optimization_guide/core/model_execution/remote_model_executor.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

// A fake implementation of OptimizationGuideService for testing.
// It allows mocking the ExecuteModel method.
class FakeOptimizationGuideService : public OptimizationGuideService {
 public:
  FakeOptimizationGuideService(
      leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
      const base::FilePath& profile_path,
      bool off_the_record,
      const std::string& application_locale,
      base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
      PrefService* pref_service,
      BrowserList* browser_list,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~FakeOptimizationGuideService() override;

  // RemoteModelExecutor implementation:
  // Executes the model for the given feature. This fake implementation will
  // look up the response or error set by `SetResponse` or `SetError`, and run
  // the callback asynchronously with that result. If neither is set, it will
  // return a generic failure.
  void ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      const optimization_guide::ModelExecutionOptions& options,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) override;

  // Helpers for testing:

  // Sets the expected successful response for a given feature.
  // The response will be serialized into an `Any` proto with the given
  // `type_name`.
  void SetResponse(optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& response,
                   const std::string& type_name);

  // Sets the expected error code for a given feature.
  void SetError(optimization_guide::ModelBasedCapabilityKey feature,
                int error_code);

 private:
  // Maps feature keys to their mocked serialized responses.
  std::map<optimization_guide::ModelBasedCapabilityKey, std::string> responses_;

  // Maps feature keys to their mocked error codes.
  std::map<optimization_guide::ModelBasedCapabilityKey, int> errors_;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_FAKE_OPTIMIZATION_GUIDE_SERVICE_H_
