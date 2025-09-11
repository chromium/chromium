// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_
#define IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"

namespace base {
class Time;
}  // namespace base

class PrefRegistrySimple;
class PrefService;

// A KeyedService that tracks user activity for cross-platform promos.
class CrossPlatformPromosService : public KeyedService {
 public:
  explicit CrossPlatformPromosService(PrefService* profile_prefs);
  ~CrossPlatformPromosService() override;

  // Registers the profile prefs used by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Called when the application enters the foreground.
  void OnApplicationWillEnterForeground();

 private:
  // Records the current day as active, and updates the pref that stores the
  // 16th most recent day that the user was active.
  void Update16thActiveDay();

  // Records the given day as an active day. Returns true if recorded, and
  // false if it had already been recorded.
  bool RecordActiveDay(base::Time day = base::Time::Now());

  // Counts back through the list of active days, starting with the most recent
  // and going to the oldest, and returns the Nth active day.
  base::Time FindActiveDay(size_t count);

  raw_ptr<PrefService> profile_prefs_;
  base::WeakPtrFactory<CrossPlatformPromosService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_CROSS_PLATFORM_PROMOS_MODEL_CROSS_PLATFORM_PROMOS_SERVICE_H_
