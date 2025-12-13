// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/chrome_profile_download_service_tracker.h"

#import "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace optimization_guide {

ChromeProfileDownloadServiceTracker::ChromeProfileDownloadServiceTracker() {
  if (auto* profile_manager = GetApplicationContext()->GetProfileManager()) {
    // Could be null in tests.
    profile_manager_observation_.Observe(profile_manager);
  }
}

ChromeProfileDownloadServiceTracker::~ChromeProfileDownloadServiceTracker() =
    default;

void ChromeProfileDownloadServiceTracker::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {
  active_profiles_.clear();
  profile_manager_observation_.Reset();
}

void ChromeProfileDownloadServiceTracker::OnProfileManagerDestroyed(
    ProfileManagerIOS* manager) {}

void ChromeProfileDownloadServiceTracker::OnProfileCreated(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {}

void ChromeProfileDownloadServiceTracker::OnProfileLoaded(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  if (profile->IsOffTheRecord()) {
    return;
  }
  active_profiles_.push_back(profile);
}

void ChromeProfileDownloadServiceTracker::OnProfileUnloaded(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  std::erase(active_profiles_, profile);
}

void ChromeProfileDownloadServiceTracker::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  std::erase(active_profiles_, profile);
}

download::BackgroundDownloadService*
ChromeProfileDownloadServiceTracker::GetBackgroundDownloadService() {
  if (active_profiles_.empty()) {
    return nullptr;
  }
  return BackgroundDownloadServiceFactory::GetForProfile(
      active_profiles_.front());
}

}  // namespace optimization_guide
