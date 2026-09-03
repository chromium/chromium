// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_EXECUTION_MANAGER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_EXECUTION_MANAGER_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

class ProfileIOS;

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

namespace private_ai {
class PrivateAiService;
}  // namespace private_ai

// A delegate for ModelExecutionManager that provides iOS-specific
// implementations.
class IOSModelExecutionManagerDelegate
    : public optimization_guide::ModelExecutionManager::Delegate {
 public:
  IOSModelExecutionManagerDelegate(
      ProfileIOS* profile,
      private_ai::PrivateAiService* private_ai_service);
  ~IOSModelExecutionManagerDelegate() override;

  // optimization_guide::ModelExecutionManager::Delegate:
  std::unique_ptr<optimization_guide::ModelExecutionFetcher>
  CreatePrivateAiFetcher() override;
  network::mojom::NetworkContext* GetNetworkContext() override;

 private:
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<private_ai::PrivateAiService> private_ai_service_;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_MODEL_EXECUTION_MANAGER_DELEGATE_H_
