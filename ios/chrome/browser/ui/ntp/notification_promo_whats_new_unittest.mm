// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"

#include <map>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/public/provider/chrome/browser/images/branded_image_icon_types.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Test fixture for NotificationPromoWhatsNew.
class NotificationPromoWhatsNewTest : public PlatformTest {
 public:
  NotificationPromoWhatsNewTest()
      : promo_(&local_state_),
        action_callback_(
            base::Bind(&NotificationPromoWhatsNewTest::OnUserAction,
                       base::Unretained(this))),
        field_trial_list_(new base::FieldTrialList(NULL)) {
    ios::NotificationPromo::RegisterPrefs(local_state_.registry());
    local_state_.registry()->RegisterInt64Pref(metrics::prefs::kInstallDate, 0);
    base::AddActionCallback(action_callback_);
  }

  ~NotificationPromoWhatsNewTest() override {
    base::RemoveActionCallback(action_callback_);
    variations::testing::ClearAllVariationParams();
  }

  void TearDown() override {
    promo_.ClearAndInitFromJson(base::DictionaryValue());
    PlatformTest::TearDown();
  }

  // Sets up a mock finch trial and inits the NotificationPromoWhatsNew. All
  // parameters will be added to the list of finch parameters.
  void Init(const std::string& start,
            const std::string& end,
            const std::string& promo_text,
            const std::string& promo_id,
            const std::string& promo_type,
            const std::string& url,
            const std::string& command,
            const std::string& metric_name,
            const std::string& icon,
            const std::string& seconds_since_install,
            const std::string& max_seconds_since_install) {
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["start"] = start;
    field_trial_params["end"] = end;
    field_trial_params["promo_text"] = promo_text;
    field_trial_params["promo_id"] = promo_id;
    field_trial_params["promo_type"] = promo_type;
    field_trial_params["url"] = url;
    field_trial_params["command"] = command;
    field_trial_params["metric_name"] = metric_name;
    field_trial_params["icon"] = icon;
    field_trial_params["seconds_since_install"] = seconds_since_install;
    field_trial_params["max_seconds_since_install"] = max_seconds_since_install;

    variations::AssociateVariationParams("IOSNTPPromotion", "Group1",
                                         field_trial_params);
    base::FieldTrialList::CreateFieldTrial("IOSNTPPromotion", "Group1");

    promo_.Init();
  }

  // Tests that |promo_text|, |promo_type|, |url|, |command_id|, and |icon|
  // equal their respective values in |promo_|, and that |valid| matches the
  // return value of |promo_|'s |CanShow()| method. |icon| is verified only if
  // |valid| is true.
  void RunTests(const std::string& promo_text,
                const std::string& promo_type,
                const std::string& url,
                const std::string& command,
                WhatsNewIcon icon,
                bool valid) {
    EXPECT_EQ(promo_text, promo_.promo_text());
    EXPECT_EQ(promo_type, promo_.promo_type());
    if (promo_type == "url")
      EXPECT_EQ(url, promo_.url().spec());
    else
      EXPECT_EQ(command, promo_.command());

    EXPECT_EQ(valid, promo_.CanShow());
    // |icon()| is set only if the promo is valid.
    if (valid)
      EXPECT_EQ(icon, promo_.icon());
  }

  void OnUserAction(const std::string& user_action) {
    user_action_count_map_[user_action]++;
  }

  int GetUserActionCount(const std::string& user_action) {
    return user_action_count_map_[user_action];
  }

 protected:
  TestingPrefServiceSimple local_state_;
  NotificationPromoWhatsNew promo_;
  base::ActionCallback action_callback_;
  std::map<std::string, int> user_action_count_map_;

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
};

// Test that a command-based, valid promo is shown with the correct text.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoCommandTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "0", "0");
  RunTests(std::string(kTestWhatsNewMessage), "chrome_command", "",
           kTestWhatsNewCommand, WHATS_NEW_LOGO, true);
}

// Test that a url-based, valid promo is shown with the correct text and icon.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoURLTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "moveToDockTip", "0",
       "url", "http://blog.chromium.org", "", "TestURLPromo", "", "0", "0");
  RunTests(l10n_util::GetStringUTF8(IDS_IOS_MOVE_TO_DOCK_TIP), "url",
           "http://blog.chromium.org/", "", WHATS_NEW_INFO, true);
}

// Test that a promo without a valid promo type is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoNoTypeTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "moveToDockTip", "0",
       "invalid type", "http://blog.chromium.org", "", "TestPromo", "", "0",
       "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that a url promo with an empty url is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoEmptyURLTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "moveToDockTip", "0",
       "url", "", "", "TestURLPromo", "", "0", "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that a url promo with an invalid url is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoInvalidURLTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "moveToDockTip", "0",
       "url", "INVALID URL", "", "TestURLPromo", "", "0", "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that a command-based promo with an invalid command is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoInvalidCommandTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", "INVALID COMMAND",
       "TestWhatsNewMetric", "logo", "0", "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that a promo without a metric name is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoNoMetricTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "moveToDockTip", "0",
       "url", "http://blog.chromium.org", "", "", "", "0", "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that if no localized text is found, the promo is not shown.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoNoLocalizedTextTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT", "TEST BAD STRING",
       "0", "url", "http://blog.chromium.org", "", "TestURLPromo", "", "0",
       "0");
  EXPECT_FALSE(promo_.CanShow());
}

// Test that if max_seconds_since_install is set, and the current time is before
// the cut off, the promo still shows.
TEST_F(NotificationPromoWhatsNewTest, MaxSecondsSinceInstallSuccessTest) {
  //  Init with max_seconds_since_install set to 2 days.
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "0", "172800");
  // Set install date to one day before now.
  base::Time one_day_before_now_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  int64_t one_day_before_now = one_day_before_now_time.ToTimeT();
  local_state_.SetInt64(metrics::prefs::kInstallDate, one_day_before_now);
  // Expect the promo to show since install date was one day ago, and the promo
  // can show until 2 days after install date.
  EXPECT_TRUE(promo_.CanShow());
}

// Test that if max_seconds_since_install is set, and the current time is after
// the cut off, the promo does not show.
TEST_F(NotificationPromoWhatsNewTest, MaxSecondsSinceInstallFailureTest) {
  //  Init with max_seconds_since_install set to 2 days.
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "0", "172800");
  // Set install date to three days before now.
  base::Time three_days_before_now_time =
      base::Time::Now() - base::TimeDelta::FromDays(3);
  int64_t three_days_before_now = three_days_before_now_time.ToTimeT();
  local_state_.SetInt64(metrics::prefs::kInstallDate, three_days_before_now);
  // Expect the promo not to show since install date was three days ago, and
  // the promo can show until 2 days after install date.
  EXPECT_FALSE(promo_.CanShow());
}
// Test that if seconds_since_install is set, and the current time is after
// install_date + seconds_since_install, the promo still shows.
TEST_F(NotificationPromoWhatsNewTest, SecondsSinceInstallSuccessTest) {
  //  Init with seconds_since_install set to 2 days.
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "172800", "0");
  // Set install date to three days before now.
  base::Time three_days_before_now_time =
      base::Time::Now() - base::TimeDelta::FromDays(3);
  int64_t three_days_before_now = three_days_before_now_time.ToTimeT();
  local_state_.SetInt64(metrics::prefs::kInstallDate, three_days_before_now);
  // Expect the promo to show since install date was three days ago, and the
  // promo can show starting at 2 days after install date.
  EXPECT_TRUE(promo_.CanShow());
}

// Test that if seconds_since_install is set, and the current time is before
// install_date + seconds_since_install, the promo does not show.
TEST_F(NotificationPromoWhatsNewTest, SecondsSinceInstallFailureTest) {
  //  Init with seconds_since_install set to 2 days.
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "172800", "0");
  // Set install date to one day before now.
  base::Time one_day_before_now_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  int64_t one_day_before_now = one_day_before_now_time.ToTimeT();
  local_state_.SetInt64(metrics::prefs::kInstallDate, one_day_before_now);
  // Expect the promo not to show since install date was one day ago, and
  // the promo can show starting at 2 days after install date.
  EXPECT_FALSE(promo_.CanShow());
}

// Test that user actions are recorded when promo is viewed and closed.
TEST_F(NotificationPromoWhatsNewTest, NotificationPromoMetricTest) {
  Init("3 Aug 1999 9:26:06 GMT", "3 Aug 2199 9:26:06 GMT",
       "testWhatsNewCommand", "0", "chrome_command", "", kTestWhatsNewCommand,
       "TestWhatsNewMetric", "logo", "0", "0");

  // Assert that promo is appropriately set up to be viewed.
  ASSERT_TRUE(promo_.CanShow());
  promo_.HandleViewed();
  EXPECT_EQ(1, GetUserActionCount("WhatsNewPromoViewed_TestWhatsNewMetric"));

  // Verify that the promo closed user action count is 0 before |HandleClosed()|
  // is called.
  EXPECT_EQ(0, GetUserActionCount("WhatsNewPromoClosed_TestWhatsNewMetric"));
  promo_.HandleClosed();
  EXPECT_EQ(1, GetUserActionCount("WhatsNewPromoClosed_TestWhatsNewMetric"));
}

}  // namespace
