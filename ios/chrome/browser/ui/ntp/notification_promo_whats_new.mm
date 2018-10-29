// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/notification_promo.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

struct PromoStringToIdsMapEntry {
  const char* promo_text_str;
  // Use |nonlocalized_message| instead of |message_id| if non-NULL.
  const char* nonlocalized_message;
  int message_id;
};

// A mapping from a string to a l10n message id.
const PromoStringToIdsMapEntry kPromoStringToIdsMap[] = {
    {"testWhatsNewCommand", kTestWhatsNewMessage, 0},
    {"moveToDockTip", NULL, IDS_IOS_MOVE_TO_DOCK_TIP},
};

// Returns a localized version of |promo_text| if it has an entry in the
// |kPromoStringToIdsMap|. If there is no entry, an empty string is returned.
std::string GetLocalizedPromoText(const std::string& promo_text) {
  for (size_t i = 0; i < base::size(kPromoStringToIdsMap); ++i) {
    auto& entry = kPromoStringToIdsMap[i];
    if (entry.promo_text_str == promo_text) {
      return entry.nonlocalized_message
                 ? std::string(entry.nonlocalized_message)
                 : l10n_util::GetStringUTF8(entry.message_id);
    }
  }
  return std::string();
}

}  // namespace

// The What's New promo command for testing.
const char kTestWhatsNewCommand[] = "testwhatsnew";
const char kTestWhatsNewMessage[] =
    "What's New? BEGIN_LINKFind out hereEND_LINK";

NotificationPromoWhatsNew::NotificationPromoWhatsNew(PrefService* local_state)
    : local_state_(local_state),
      valid_(false),
      notification_promo_(local_state_) {}

NotificationPromoWhatsNew::~NotificationPromoWhatsNew() {}

bool NotificationPromoWhatsNew::Init() {
  notification_promo_.InitFromVariations();

  // Force enable a particular promo if experimental flag is set.
  switch (experimental_flags::GetWhatsNewPromoStatus()) {
    case experimental_flags::WHATS_NEW_DEFAULT:
      // Do nothing. Use default experiment.
      break;
    case experimental_flags::WHATS_NEW_TEST_COMMAND_TIP:
      InjectFakePromo("1", "testWhatsNewCommand", "chrome_command",
                      kTestWhatsNewCommand, "", "TestWhatsNewCommand", "logo");
      break;
    case experimental_flags::WHATS_NEW_MOVE_TO_DOCK_TIP:
      InjectFakePromo("2", "moveToDockTip", "url", "",
                      "https://support.google.com/chrome/?p=iphone_dock&ios=1",
                      "MoveToDockTipPromo", "logoWithRoundedRectangle");
      break;
    default:
      NOTREACHED();
      break;
  }

  notification_promo_.InitFromPrefs();
  return InitFromNotificationPromo();
}

bool NotificationPromoWhatsNew::ClearAndInitFromJson(
    const base::DictionaryValue& json) {
  // This clears away old promos.
  notification_promo_.MigrateUserPrefs(local_state_);

  notification_promo_.InitFromJson(json);
  return InitFromNotificationPromo();
}

bool NotificationPromoWhatsNew::CanShow() const {
  if (!valid_ || !notification_promo_.CanShow()) {
    return false;
  }

  // Check optional restrictions.

  if (seconds_since_install_ > 0) {
    // Do not show the promo if the app's installation did not occur more than
    // |seconds_since_install_| seconds ago.
    int64_t install_date = local_state_->GetInt64(metrics::prefs::kInstallDate);
    const base::Time first_view_time =
        base::Time::FromTimeT(install_date) +
        base::TimeDelta::FromSeconds(seconds_since_install_);
    if (first_view_time > base::Time::Now()) {
      return false;
    }
  }

  if (max_seconds_since_install_ > 0) {
    // Do not show the promo if the app's installation occurred more than
    // |max_seconds_since_install_| seconds ago.
    int64_t install_date = local_state_->GetInt64(metrics::prefs::kInstallDate);
    const base::Time last_view_time =
        base::Time::FromTimeT(install_date) +
        base::TimeDelta::FromSeconds(max_seconds_since_install_);
    if (last_view_time < base::Time::Now()) {
      return false;
    }
  }

  return true;
}

void NotificationPromoWhatsNew::HandleViewed() {
  // TODO(justincohen): metrics actions names should be inlined. Since
  // metric_name_ is retrieved from a server, it's not possible to know at build
  // time the values. We should investigate to find a solution. In the meantime,
  // metrics will be reported hashed.
  // http://crbug.com/437500
  std::string metric = std::string("WhatsNewPromoViewed_") + metric_name_;
  base::RecordAction(base::UserMetricsAction(metric.c_str()));
  base::RecordAction(base::UserMetricsAction("NTPPromoShown"));
  notification_promo_.HandleViewed();
}

void NotificationPromoWhatsNew::HandleClosed() {
  // TODO(justincohen): metrics actions names should be inlined. Since
  // metric_name_ is retrieved from a server, it's not possible to know at build
  // time the values. We should investigate to find a solution. In the meantime,
  // metrics will be reported hashed.
  // http://crbug.com/437500
  std::string metric = std::string("WhatsNewPromoClosed_") + metric_name_;
  base::RecordAction(base::UserMetricsAction(metric.c_str()));
  base::RecordAction(base::UserMetricsAction("NTPPromoClosed"));
  notification_promo_.HandleClosed();
}

bool NotificationPromoWhatsNew::IsURLPromo() const {
  return promo_type_ == "url";
}

bool NotificationPromoWhatsNew::IsChromeCommandPromo() const {
  return promo_type_ == "chrome_command";
}

WhatsNewIcon NotificationPromoWhatsNew::ParseIconName(
    const std::string& icon_name) {
  if (icon_name == "logo") {
    return WHATS_NEW_LOGO;
  } else if (icon_name == "logoWithRoundedRectangle") {
    return WHATS_NEW_LOGO_ROUNDED_RECTANGLE;
  }
  return WHATS_NEW_INFO;
}

bool NotificationPromoWhatsNew::InitFromNotificationPromo() {
  valid_ = false;

  promo_text_ = GetLocalizedPromoText(notification_promo_.promo_text());
  if (promo_text_.empty())
    return valid_;

  notification_promo_.promo_payload()->GetString("metric_name", &metric_name_);
  if (metric_name_.empty())
    return valid_;

  notification_promo_.promo_payload()->GetString("promo_type", &promo_type_);
  if (IsURLPromo()) {
    std::string url_text;
    notification_promo_.promo_payload()->GetString("url", &url_text);
    url_ = GURL(url_text);
    if (url_.is_empty() || !url_.is_valid()) {
      return valid_;
    }
  } else if (IsChromeCommandPromo()) {
    notification_promo_.promo_payload()->GetString("command", &command_);
    // There is only one valid command for NTP Promotions, and that is the
    // test command itself.
    if (command_ != kTestWhatsNewCommand) {
      return valid_;
    }
  } else {  // If |promo_type_| is not set to URL or Command, return early.
    return valid_;
  }

  valid_ = true;

  // Optional values don't need validation.
  std::string icon_name;
  notification_promo_.promo_payload()->GetString("icon", &icon_name);
  icon_ = ParseIconName(icon_name);

  seconds_since_install_ = 0;
  notification_promo_.promo_payload()->GetInteger("seconds_since_install",
                                                  &seconds_since_install_);
  max_seconds_since_install_ = 0;
  notification_promo_.promo_payload()->GetInteger("max_seconds_since_install",
                                                  &max_seconds_since_install_);
  return valid_;
}

void NotificationPromoWhatsNew::InjectFakePromo(const std::string& promo_id,
                                                const std::string& promo_text,
                                                const std::string& promo_type,
                                                const std::string& command,
                                                const std::string& url,
                                                const std::string& metric_name,
                                                const std::string& icon) {
  // Build vector to fill in json string with given parameters.
  std::vector<std::string> replacements;
  replacements.push_back(promo_text);
  replacements.push_back(promo_type);
  replacements.push_back(metric_name);
  replacements.push_back(command);
  replacements.push_back(url);
  replacements.push_back(icon);
  replacements.push_back(promo_id);

  std::string promo_json =
      "{"
      "  \"start\":\"1 Jan 1999 0:26:06 GMT\","
      "  \"end\":\"1 Jan 2199 0:26:06 GMT\","
      "  \"promo_text\":\"$1\","
      "  \"max_views\":20,"
      "  \"payload\":"
      "     {"
      "       \"promo_type\":\"$2\","
      "       \"metric_name\":\"$3\","
      "       \"command\":\"$4\","
      "       \"url\":\"$5\","
      "       \"icon\":\"$6\""
      "     },"
      "  \"max_seconds\":259200,"
      "  \"promo_id\":$7"
      "}";
  std::string promo_json_filled_in =
      base::ReplaceStringPlaceholders(promo_json, replacements, NULL);

  std::unique_ptr<base::Value> value(
      base::JSONReader::Read(promo_json_filled_in));
  base::DictionaryValue* dict = NULL;
  if (value->GetAsDictionary(&dict)) {
    notification_promo_.InitFromJson(*dict);
  }
}
