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
  // Subscribe before `OnPrefsUpdated()`, which may already build a `Scorer`.
  scorer_update_subscription_ = RegisterCallbackForScorerUpdates(
      base::BindRepeating(&ClientSideDetectionService::NotifyScorerChanged,
                          base::Unretained(this)));
  OnPrefsUpdated();
}

ClientSideDetectionService::~ClientSideDetectionService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unsubscribe before the base class clears the `Scorer` in its destructor so
  // that `observers_` is not notified while this object is partially
  // destroyed.
  scorer_update_subscription_ = base::CallbackListSubscription();
}

bool ClientSideDetectionService::ShouldCreateBrowserScorer() const {
  return true;
}

void ClientSideDetectionService::NotifyScorerChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_) {
    observer.OnScorerChanged();
  }
}

void ClientSideDetectionService::
    SetSharedURLLoaderFactoryForTesting(  // IN-TEST
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_factory_ = std::move(url_loader_factory);
}

void ClientSideDetectionService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ClientSideDetectionService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
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
