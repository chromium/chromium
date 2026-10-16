// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "components/ttc/app/ttc_backend.h"
#import "components/ttc/app/ttc_mes_client.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"

TTCKeyedService::TTCKeyedService(OptimizationGuideService* optimization_guide,
                                 BrowserList* browser_list)
    : optimization_guide_(optimization_guide), browser_list_(browser_list) {}

TTCKeyedService::~TTCKeyedService() = default;

void TTCKeyedService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndSession();
  browser_list_ = nullptr;
}

bool TTCKeyedService::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsTTCEnabled();
}

bool TTCKeyedService::is_session_active() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_controller_ != nullptr &&
         session_controller_->lifecycle() != TTCSessionLifecycle::kFinished;
}

TtcSessionController* TTCKeyedService::session_controller() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_session_active() ? session_controller_.get() : nullptr;
}

TTCServiceState TTCKeyedService::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnabled()) {
    return TTCServiceState::kProfileIneligible;
  }
  return is_session_active() ? TTCServiceState::kSessionActive
                             : TTCServiceState::kSessionInactive;
}

TTCSessionLifecycle TTCKeyedService::GetSessionLifecycle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_controller_) {
    return TTCSessionLifecycle::kFinished;
  }
  return session_controller_->lifecycle();
}

void TTCKeyedService::StartSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsEnabled());
  CHECK(!is_session_active());
  if (session_controller_) {
    session_controller_->Disconnect();
    session_controller_.reset();
  }
  journal_.Clear();

  const ios::provider::TTCConfig config = ios::provider::GetTTCConfig();
  std::unique_ptr<ttc::TtcBackend> backend;
  if (config.is_valid() && !config.api_key.empty()) {
    backend = std::make_unique<TtcWebSocketBackend>();
  } else {
    backend = std::make_unique<ttc::TtcMesClient>(optimization_guide_.get());
  }

  session_controller_ =
      std::make_unique<TtcSessionController>(std::move(backend), browser_list_);
  session_controller_->set_journal(&journal_);

  session_controller_->StartSession();
  state_changed_callbacks_.Notify(GetState());
}

void TTCKeyedService::EndSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_session_active() && !session_controller_) {
    return;
  }
  std::unique_ptr<TtcSessionController> controller =
      std::move(session_controller_);
  controller->Disconnect();
  state_changed_callbacks_.Notify(GetState());
}

base::CallbackListSubscription TTCKeyedService::RegisterStateChangedCallback(
    base::RepeatingCallback<void(TTCServiceState)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_changed_callbacks_.Add(std::move(callback));
}

base::WeakPtr<TTCKeyedService> TTCKeyedService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}
