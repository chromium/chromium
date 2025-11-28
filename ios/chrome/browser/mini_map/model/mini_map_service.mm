// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_service.h"

#import "base/functional/callback_helpers.h"
#import "components/google/core/common/google_util.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Checks if the Default Search Engine is Google.
bool IsGoogleDSE(TemplateURLService* template_url_service) {
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    return false;
  }
  return default_provider->GetEngineType(
             template_url_service->search_terms_data()) ==
         SearchEngineType::SEARCH_ENGINE_GOOGLE;
}

}  // namespace

MiniMapService::MiniMapService(PrefService* pref_service,
                               TemplateURLService* template_url_service,
                               signin::IdentityManager* identity_manager)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      identity_manager_(identity_manager) {
  template_url_service_->AddObserver(this);
  is_dse_google_ = IsGoogleDSE(template_url_service_);

  is_signed_in_ =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  identity_manager->AddObserver(this);

  mini_map_enabled_pref_.Init(
      prefs::kIosMiniMapShowNativeMap, pref_service,
      base::BindRepeating(&MiniMapService::OnPrefChanged,
                          weak_factory_.GetWeakPtr()));
  is_mini_map_enabled_ = mini_map_enabled_pref_.GetValue();

  // Start the application observer
  MiniMapService::MiniMapAppObserver::GetInstance();
}

MiniMapService::~MiniMapService() {}

void MiniMapService::Shutdown() {
  mini_map_enabled_pref_.Destroy();
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
    template_url_service_ = nullptr;
  }
}

bool MiniMapService::IsMiniMapEnabled() {
  return is_mini_map_enabled_;
}

// Whether the current default search engine is Google.
bool MiniMapService::IsDSEGoogle() {
  return is_dse_google_;
}

bool MiniMapService::IsSignedIn() {
  return is_signed_in_;
}

// static
bool MiniMapService::IsGoogleMapsInstalled() {
  return MiniMapService::MiniMapAppObserver::GetInstance()
      ->IsGoogleMapsInstalled();
}

#pragma mark - TemplateURLServiceObserver

void MiniMapService::OnTemplateURLServiceChanged() {
  is_dse_google_ = IsGoogleDSE(template_url_service_);
}

void MiniMapService::OnTemplateURLServiceShuttingDown() {
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
    template_url_service_ = nullptr;
  }
}

#pragma mark - signin::IdentityManager::Observer

void MiniMapService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  is_signed_in_ =
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

void MiniMapService::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  is_signed_in_ = false;
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }
}

#pragma mark - Private

void MiniMapService::OnPrefChanged() {
  is_mini_map_enabled_ = mini_map_enabled_pref_.GetValue();
}

#pragma mark - MiniMapService::MiniMapAppObserver

// static
MiniMapService::MiniMapAppObserver*
MiniMapService::MiniMapAppObserver::GetInstance() {
  static base::NoDestructor<MiniMapService::MiniMapAppObserver> instance;
  return instance.get();
}

MiniMapService::MiniMapAppObserver::MiniMapAppObserver() {
  base::RepeatingCallback<void(NSNotification*)> foregrounding_closure =
      base::IgnoreArgs<NSNotification*>(base::BindRepeating(
          &MiniMapService::MiniMapAppObserver::OnAppDidBecomeActive,
          base::Unretained(this)));

  foreground_notification_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(foregrounding_closure)];
  is_google_maps_installed_ = IsGoogleMapsAppInstalled();
}

MiniMapService::MiniMapAppObserver::~MiniMapAppObserver() {}

void MiniMapService::MiniMapAppObserver::OnAppDidBecomeActive() {
  is_google_maps_installed_ = IsGoogleMapsAppInstalled();
}

bool MiniMapService::MiniMapAppObserver::IsGoogleMapsInstalled() {
  return is_google_maps_installed_;
}
