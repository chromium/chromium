// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

ClientSideDetectionService::ClientSideDetectionService(
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : ClientSideDetectionServiceBase(prefs, opt_guide) {
  CHECK(prefs);
  url_loader_factory_ = std::move(url_loader_factory);
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ClientSideDetectionService::ShouldCreateBrowserScorer() const {
  return true;
}

void ClientSideDetectionService::
    SetSharedURLLoaderFactoryForTesting(  // IN-TEST
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_factory_ = std::move(url_loader_factory);
}

void ClientSideDetectionService::DidSendClientReportPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> request,
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIIOSInfoSingleton::AddToClientPhishingRequestsSent,
                     base::Unretained(WebUIIOSInfoSingleton::GetInstance()),
                     std::move(request), access_token));
}

void ClientSideDetectionService::DidReceiveClientPhishingResponse(
    const ClientPhishingResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebUIIOSInfoSingleton::AddToClientPhishingResponsesReceived,
          base::Unretained(WebUIIOSInfoSingleton::GetInstance()),
          std::make_unique<ClientPhishingResponse>(response)));
}

}  // namespace safe_browsing
