// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NOTIFICATION_PROMO_WHATS_NEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_NOTIFICATION_PROMO_WHATS_NEW_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ios/chrome/browser/notification_promo.h"
#include "ios/public/provider/chrome/browser/images/branded_image_icon_types.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

// The What's New promo command for testing.
extern const char kTestWhatsNewCommand[];
extern const char kTestWhatsNewMessage[];

// Helper class for NotificationPromo that deals with mobile_ntp promos.
class NotificationPromoWhatsNew {
 public:
  explicit NotificationPromoWhatsNew(PrefService* local_state);
  ~NotificationPromoWhatsNew();

  // Initialize from variations/prefs/JSON.
  // Return true if the mobile NTP promotion is valid.
  bool Init();

  // Used by experimental setting to always show a promo.
  bool ClearAndInitFromJson(const base::DictionaryValue& json);

  // Return true if the promo is valid and can be shown.
  bool CanShow() const;

  // Mark the promo as closed when the user dismisses it.
  void HandleClosed();

  // Mark the promo as having been viewed.
  void HandleViewed();

  bool valid() const { return valid_; }
  const std::string& promo_type() { return promo_type_; }
  const std::string& promo_text() { return promo_text_; }
  WhatsNewIcon icon() { return icon_; }
  bool IsURLPromo() const;
  const GURL& url() { return url_; }
  bool IsChromeCommandPromo() const;
  const std::string& command() { return command_; }

 private:
  // Initialize the state and validity from the low-level notification_promo_.
  bool InitFromNotificationPromo();

  // Inject a fake promo. The parameters are equivalent to the equivalent
  // parameters that can be provided by the variations API. In addition, for
  // some variations parameters that are not in this list, the following
  // defaults are used: start: 1 Jan 1999 0:26:06 GMT,
  // end: 1 Jan 2199 0:26:06 GMT, max_views: 20, max_seconds: 259200.
  void InjectFakePromo(const std::string& promo_id,
                       const std::string& promo_text,
                       const std::string& promo_type,
                       const std::string& command,
                       const std::string& url,
                       const std::string& metric_name,
                       const std::string& icon);

  // Prefs service for promos.
  PrefService* local_state_;

  // True if InitFromPrefs/JSON was called and all mandatory fields were found.
  bool valid_;

  // Text of promo.
  std::string promo_text_;

  // Type of whats new promo.
  std::string promo_type_;

  // Icon of promo.
  WhatsNewIcon icon_;

  // The minimum number of seconds from installation before promo can be valid.
  // E.g. Don't show the promo if installation was within N days.
  int seconds_since_install_;

  // The duration after installation that the promo can be valid.
  // E.g. Don't show the promo if installation was more than N days ago.
  int max_seconds_since_install_;

  // If promo type is 'url'.
  GURL url_;

  // If promo type is 'chrome_command'.
  std::string command_;

  // Metric name to append
  std::string metric_name_;

  // The lower-level notification promo.
  ios::NotificationPromo notification_promo_;

  // Convert an icon name string to WhatsNewIcon.
  WhatsNewIcon ParseIconName(const std::string& icon_name);

  DISALLOW_COPY_AND_ASSIGN(NotificationPromoWhatsNew);
};

#endif  // IOS_CHROME_BROWSER_UI_NTP_NOTIFICATION_PROMO_WHATS_NEW_H_
