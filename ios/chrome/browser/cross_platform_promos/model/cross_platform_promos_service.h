// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_
#define IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"

namespace base {
class Time;
}  // namespace base

class Browser;
class ProfileIOS;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// A KeyedService that tracks user activity for cross-platform promos.
class CrossPlatformPromosService : public KeyedService {
 public:
  CrossPlatformPromosService(ProfileIOS* profile);
  ~CrossPlatformPromosService() override;

  // Registers the profile prefs used by this service.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Called when the application enters the foreground.
  void OnApplicationWillEnterForeground();

  // Shows the Lens promo.
  void ShowLensPromo(Browser* browser);

  // Shows the Enhanced Safe Browsing promo.
  void ShowESBPromo(Browser* browser);

  // Shows the CPE promo.
  void ShowCPEPromo(Browser* browser);

  // Evaluates synced prefs to see whether a promo should be shown.
  void MaybeShowPromo();

 private:
  // Returns a regular, active browser, or nullptr if none is found.
  Browser* GetActiveBrowser();

  // Records the current day as active, and updates the pref that stores the
  // 16th most recent day that the user was active.
  void Update16thActiveDay();

  // Records the given day as an active day. Returns true if recorded, and
  // false if it had already been recorded.
  bool RecordActiveDay(base::Time day = base::Time::Now());

  // Counts back through the list of active days, starting with the most recent
  // and going to the oldest, and returns the Nth active day.
  base::Time FindActiveDay(size_t count);

  raw_ptr<ProfileIOS> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  base::WeakPtrFactory<CrossPlatformPromosService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_
