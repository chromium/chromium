// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_

#include <vector>

#import "base/scoped_observation.h"
#import "components/optimization_guide/core/delivery/profile_download_service_tracker.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"

namespace optimization_guide {

class ChromeProfileDownloadServiceTrackerIOSTest;

class ChromeProfileDownloadServiceTracker
    : public ProfileDownloadServiceTracker,
      public ProfileManagerObserverIOS {
 public:
  ChromeProfileDownloadServiceTracker();
  ~ChromeProfileDownloadServiceTracker() override;

  ChromeProfileDownloadServiceTracker(
      const ChromeProfileDownloadServiceTracker&) = delete;
  ChromeProfileDownloadServiceTracker& operator=(
      const ChromeProfileDownloadServiceTracker&) = delete;

  // ProfileDownloadServiceTracker:
  download::BackgroundDownloadService* GetBackgroundDownloadService() override;

 private:
  friend class ChromeProfileDownloadServiceTrackerIOSTest;

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager, ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager, ProfileIOS* profile) override;
  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override;
  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override;

  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};

  // The list of regular profiles that are currently active. This list is used
  // to determine which profile to use for model downloads. Kept in the order
  // that the profiles were added.
  std::vector<ProfileIOS*> active_profiles_;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_CHROME_PROFILE_DOWNLOAD_SERVICE_TRACKER_H_
