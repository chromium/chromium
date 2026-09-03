// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/ios_model_execution_manager_delegate.h"

#import "base/check.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
#import "components/optimization_guide/core/model_execution/private_ai_model_execution_fetcher.h"
#import "components/private_ai/client.h"
#import "components/private_ai/private_ai_service.h"
#endif

IOSModelExecutionManagerDelegate::IOSModelExecutionManagerDelegate(
    ProfileIOS* profile,
    private_ai::PrivateAiService* private_ai_service)
    : profile_(profile), private_ai_service_(private_ai_service) {
  CHECK(profile_);
}

IOSModelExecutionManagerDelegate::~IOSModelExecutionManagerDelegate() = default;

std::unique_ptr<optimization_guide::ModelExecutionFetcher>
IOSModelExecutionManagerDelegate::CreatePrivateAiFetcher() {
#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
  if (!private_ai_service_) {
    return nullptr;
  }
  private_ai::Client* client = private_ai_service_->GetClient();
  return std::make_unique<optimization_guide::PrivateAiModelExecutionFetcher>(
      client);
#else
  return nullptr;
#endif
}

network::mojom::NetworkContext*
IOSModelExecutionManagerDelegate::GetNetworkContext() {
  return profile_->GetNetworkContext();
}
