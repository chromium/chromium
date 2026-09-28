// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"

#import <utility>

#import "base/check.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

// static
TTCKeyedService* TTCKeyedService::Get(ProfileIOS* profile) {
  return TTCKeyedServiceFactory::GetForProfile(profile);
}

TTCKeyedService::TTCKeyedService(ProfileIOS* profile) : profile_(profile) {
  CHECK(profile_);
}

TTCKeyedService::~TTCKeyedService() = default;

void TTCKeyedService::Shutdown() {
  EndSession();
}

bool TTCKeyedService::IsEnabled() const {
  return IsTTCEnabled();
}

TTCServiceState TTCKeyedService::GetState() const {
  if (!IsEnabled()) {
    return TTCServiceState::kProfileIneligible;
  }
  return is_session_active_ ? TTCServiceState::kSessionActive
                            : TTCServiceState::kSessionInactive;
}

void TTCKeyedService::StartSession() {
  CHECK(IsEnabled());
  CHECK(!is_session_active_);
  is_session_active_ = true;
  state_changed_callbacks_.Notify(GetState());
}

void TTCKeyedService::EndSession() {
  if (!is_session_active_) {
    return;
  }
  is_session_active_ = false;
  state_changed_callbacks_.Notify(GetState());
}

base::CallbackListSubscription TTCKeyedService::RegisterStateChangedCallback(
    base::RepeatingCallback<void(TTCServiceState)> callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

base::WeakPtr<TTCKeyedService> TTCKeyedService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
