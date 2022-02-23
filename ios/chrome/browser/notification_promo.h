// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_
#define IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_

#include <memory>
#include <string>

#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios {

extern const char kNTPPromoFinchExperiment[];

// Class that parses and manages promotion data from either a finch trial, json,
// or prefs.
class NotificationPromo {
 public:
  explicit NotificationPromo(PrefService* local_state);

  NotificationPromo(const NotificationPromo&) = delete;
  NotificationPromo& operator=(const NotificationPromo&) = delete;

  ~NotificationPromo();

  // Initialize from finch parameters.
  void InitFromVariations();

  // Initialize from json/prefs.
  void InitFromJson(base::Value json);
  void InitFromPrefs();

  // Can this promo be shown?
  bool CanShow() const;

  // The time when this promo can start being viewed.
  double StartTime() const;
  // The time after which this promo no longer can be viewed.
  double EndTime() const;

  // Mark the promo as closed when the user dismisses it.
  void HandleClosed();
  // Mark the promo has having been viewed.
  void HandleViewed();

  const std::string& promo_text() const { return promo_text_; }
  const base::Value& promo_payload() const {
    DCHECK(promo_payload_.is_dict());
    return promo_payload_;
  }

  // Register preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void MigrateUserPrefs(PrefService* user_prefs);

 private:
  // For testing.
  friend class NotificationPromoTest;

  // Flush data from instance variables to prefs for storage.
  void WritePrefs();

  // Flush given parameters to prefs for storage.
  void WritePrefs(int promo_id, double first_view_time, int views, bool closed);

  // Tests views_ against max_views_.
  // When max_views_ is 0, we don't cap the number of views.
  bool ExceedsMaxViews() const;

  // Tests |first_view_time_| + |max_seconds_| and -now().
  // When either is 0, we don't cap the number of seconds.
  bool ExceedsMaxSeconds() const;

  // Returns whether the parameter associated with |param_name| is inside the
  // payload.
  bool IsPayloadParam(const std::string& param_name) const;

  PrefService* local_state_;

  std::string promo_text_;

  base::Value promo_payload_;

  double start_;
  double end_;
  int promo_id_;

  // When max_views_ is 0, we don't cap the number of views.
  int max_views_;

  // When max_seconds_ is 0 or not set, we don't cap the number of seconds a
  // promo can be visible.
  int max_seconds_;

  // Set when the promo is viewed for the first time.
  double first_view_time_;

  int views_;
  bool closed_;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_NOTIFICATION_PROMO_H_
