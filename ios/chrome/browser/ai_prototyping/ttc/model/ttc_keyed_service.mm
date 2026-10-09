// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"

#import <memory>
#import <utility>

#import "base/check.h"
#import "components/ttc/app/ttc_backend.h"
#import "components/ttc/app/ttc_mes_client.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_api.h"

// static
TTCKeyedService* TTCKeyedService::Get(ProfileIOS* profile) {
  return TTCKeyedServiceFactory::GetForProfile(profile);
}

TTCKeyedService::TTCKeyedService(ProfileIOS* profile) : profile_(profile) {
  CHECK(profile_);
}

TTCKeyedService::~TTCKeyedService() = default;

void TTCKeyedService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndSession();
}

bool TTCKeyedService::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return IsTTCEnabled();
}

bool TTCKeyedService::is_session_active() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_controller_ != nil &&
         session_controller_.lifecycle != TTCSessionLifecycle::kFinished;
}

TTCSessionController* TTCKeyedService::session_controller() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_session_active() ? session_controller_ : nil;
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
  return session_controller_.lifecycle;
}

void TTCKeyedService::StartSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsEnabled());
  CHECK(!is_session_active());
  if (session_controller_) {
    [session_controller_ disconnect];
    session_controller_ = nil;
  }

  const ios::provider::TTCConfig config = ios::provider::GetTTCConfig();
  std::unique_ptr<ttc::TtcBackend> backend;
  if (config.is_valid() && !config.api_key.empty()) {
    backend = std::make_unique<TtcWebSocketBackend>();
  } else {
    backend = std::make_unique<ttc::TtcMesClient>(
        OptimizationGuideServiceFactory::GetForProfile(profile_));
  }

  session_controller_ =
      [[TTCSessionController alloc] initWithBackend:std::move(backend)];
  CHECK(session_controller_);

  [session_controller_ startSession];
  state_changed_callbacks_.Notify(GetState());
}

void TTCKeyedService::EndSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_session_active() && !session_controller_) {
    return;
  }
  TTCSessionController* controller = session_controller_;
  session_controller_ = nil;
  [controller disconnect];
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
