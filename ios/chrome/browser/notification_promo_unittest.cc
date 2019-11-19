// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/notification_promo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"

namespace ios {

namespace {

const char kDateFormat[] = "dd MMM yyyy HH:mm:ss zzzz";

bool YearFromNow(double* date_epoch, std::string* date_string) {
  *date_epoch = (base::Time::Now() + base::TimeDelta::FromDays(365)).ToTimeT();

  UErrorCode status = U_ZERO_ERROR;
  icu::SimpleDateFormat simple_formatter(icu::UnicodeString(kDateFormat),
                                         icu::Locale("en_US"), status);
  icu::UnicodeString date_unicode_string;
  simple_formatter.format(static_cast<UDate>(*date_epoch * 1000),
                          date_unicode_string, status);
  if (U_FAILURE(status))
    return false;

  date_unicode_string.toUTF8String(*date_string);
  return true;
}

}  // namespace

class NotificationPromoTest : public PlatformTest {
 public:
  NotificationPromoTest()
      : notification_promo_(&local_state_),
        received_notification_(false),
        start_(0.0),
        end_(0.0),
        promo_id_(-1),
        max_views_(0),
        max_seconds_(0),
        closed_(false) {
    NotificationPromo::RegisterPrefs(local_state_.registry());
  }
  ~NotificationPromoTest() override {
    variations::testing::ClearAllVariationParams();
  }

  void Init(const std::string& json,
            const std::string& promo_text,
            double start,
            int promo_id,
            int max_views,
            int max_seconds) {
    double year_from_now_epoch;
    std::string year_from_now_string;
    ASSERT_TRUE(YearFromNow(&year_from_now_epoch, &year_from_now_string));

    std::vector<std::string> replacements;
    replacements.push_back(year_from_now_string);

    std::string json_with_end_date(
        base::ReplaceStringPlaceholders(json, replacements, NULL));
    base::Optional<base::Value> value =
        base::JSONReader::Read(json_with_end_date);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value.value().is_dict());

    test_json_ = std::move(value).value();

    const std::string* start_param = test_json_.FindStringKey("start");
    ASSERT_TRUE(start_param);

    std::map<std::string, std::string> field_trial_params;
    field_trial_params["start"] = *start_param;
    field_trial_params["end"] = year_from_now_string;
    field_trial_params["promo_text"] = promo_text;
    field_trial_params["max_views"] = base::NumberToString(max_views);
    field_trial_params["max_seconds"] = base::NumberToString(max_seconds);
    field_trial_params["promo_id"] = base::NumberToString(promo_id);

    // Payload parameters.
    base::Value* payload =
        test_json_.FindKeyOfType("payload", base::Value::Type::DICTIONARY);
    ASSERT_TRUE(payload);
    ASSERT_TRUE(payload->is_dict());

    for (const auto& pair : payload->DictItems()) {
      field_trial_params[pair.first] =
          pair.second.is_string() ? pair.second.GetString() : std::string();
    }

    variations::AssociateVariationParams("IOSNTPPromotion", "Group1",
                                         field_trial_params);
    base::FieldTrialList::CreateFieldTrial("IOSNTPPromotion", "Group1");

    promo_text_ = promo_text;

    start_ = start;
    end_ = year_from_now_epoch;

    promo_id_ = promo_id;
    max_views_ = max_views;
    max_seconds_ = max_seconds;

    closed_ = false;
    received_notification_ = false;
  }

  void InitPromoFromVariations() {
    notification_promo_.InitFromVariations();

    // Test the fields.
    TestServerProvidedParameters();
  }

  void InitPromoFromJson() {
    notification_promo_.InitFromJson(test_json_.Clone());

    // Test the fields.
    TestServerProvidedParameters();
  }

  void TestServerProvidedParameters() {
    // Check values.
    EXPECT_EQ(notification_promo_.promo_text_, promo_text_);

    EXPECT_DOUBLE_EQ(notification_promo_.start_, start_);
    EXPECT_DOUBLE_EQ(notification_promo_.end_, end_);

    EXPECT_EQ(notification_promo_.promo_id_, promo_id_);
    EXPECT_EQ(notification_promo_.max_views_, max_views_);
    EXPECT_EQ(notification_promo_.max_seconds_, max_seconds_);
  }

  void TestViews() {
    notification_promo_.views_ = notification_promo_.max_views_ - 2;
    notification_promo_.WritePrefs();

    // Initialize promo from saved prefs and server params.
    NotificationPromo first_promo(&local_state_);
    first_promo.InitFromVariations();
    first_promo.InitFromPrefs();
    EXPECT_EQ(first_promo.max_views_ - 2, first_promo.views_);
    EXPECT_TRUE(first_promo.CanShow());
    first_promo.HandleViewed();

    // Initialize another promo to test that the new views were recorded
    // correctly in prefs.
    NotificationPromo second_promo(&local_state_);
    second_promo.InitFromVariations();
    second_promo.InitFromPrefs();
    EXPECT_EQ(second_promo.max_views_ - 1, second_promo.views_);
    EXPECT_TRUE(second_promo.CanShow());
    second_promo.HandleViewed();

    NotificationPromo third_promo(&local_state_);
    third_promo.InitFromVariations();
    third_promo.InitFromPrefs();
    EXPECT_EQ(third_promo.max_views_, third_promo.views_);
    EXPECT_FALSE(third_promo.CanShow());

    // Test out of range views.
    for (int i = max_views_; i < max_views_ * 2; ++i) {
      third_promo.views_ = i;
      EXPECT_FALSE(third_promo.CanShow());
    }

    // Test in range views.
    for (int i = 0; i < max_views_; ++i) {
      third_promo.views_ = i;
      EXPECT_TRUE(third_promo.CanShow());
    }

    // Reset prefs to default.
    notification_promo_.views_ = 0;
    notification_promo_.WritePrefs();
  }

  void TestClosed() {
    // Initialize promo from saved prefs and server params.
    NotificationPromo first_promo(&local_state_);
    first_promo.InitFromVariations();
    first_promo.InitFromPrefs();
    EXPECT_FALSE(first_promo.closed_);
    EXPECT_TRUE(first_promo.CanShow());
    first_promo.HandleClosed();

    // Initialize another promo to test that the the closing of the promo was
    // recorded correctly in prefs.
    NotificationPromo second_promo(&local_state_);
    second_promo.InitFromVariations();
    second_promo.InitFromPrefs();
    EXPECT_TRUE(second_promo.closed_);
    EXPECT_FALSE(second_promo.CanShow());

    // Reset prefs to default.
    second_promo.closed_ = false;
    EXPECT_TRUE(second_promo.CanShow());
    second_promo.WritePrefs();
  }

  void TestPromoText() {
    notification_promo_.promo_text_.clear();
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.promo_text_ = promo_text_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  void TestTime() {
    const double now = base::Time::Now().ToDoubleT();
    const double qhour = 15 * 60;

    notification_promo_.start_ = now - qhour;
    notification_promo_.end_ = now + qhour;
    EXPECT_TRUE(notification_promo_.CanShow());

    // Start time has not arrived.
    notification_promo_.start_ = now + qhour;
    notification_promo_.end_ = now + qhour;
    EXPECT_FALSE(notification_promo_.CanShow());

    // End time has past.
    notification_promo_.start_ = now - qhour;
    notification_promo_.end_ = now - qhour;
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.start_ = start_;
    notification_promo_.end_ = end_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  void TestMaxTime() {
    const double now = base::Time::Now().ToDoubleT();
    const double margin = 60;

    // Current time is before the |first_view_time_| + |max_seconds_|.
    notification_promo_.first_view_time_ = now - margin;
    notification_promo_.max_seconds_ = margin + 1;
    EXPECT_TRUE(notification_promo_.CanShow());

    // Current time as after the |first_view_time_| + |max_seconds_|.
    notification_promo_.first_view_time_ = now - margin;
    notification_promo_.max_seconds_ = margin - 1;
    EXPECT_FALSE(notification_promo_.CanShow());

    notification_promo_.first_view_time_ = 0;
    notification_promo_.max_seconds_ = max_seconds_;
    EXPECT_TRUE(notification_promo_.CanShow());
  }

  // Tests that the first view time is recorded properly in prefs when the
  // first view occurs.
  void TestFirstViewTimeRecorded() {
    EXPECT_DOUBLE_EQ(0, notification_promo_.first_view_time_);
    notification_promo_.HandleViewed();

    NotificationPromo temp_promo(&local_state_);
    temp_promo.InitFromVariations();
    temp_promo.InitFromPrefs();
    EXPECT_NE(0, temp_promo.first_view_time_);

    notification_promo_.views_ = 0;
    notification_promo_.first_view_time_ = 0;
    notification_promo_.WritePrefs();
  }

  const NotificationPromo& promo() const { return notification_promo_; }

 protected:
  TestingPrefServiceSimple local_state_;

 private:
  NotificationPromo notification_promo_;
  bool received_notification_;
  base::Value test_json_;

  std::string promo_text_;

  double start_;
  double end_;
  int promo_id_;
  int max_views_;
  int max_seconds_;

  bool closed_;
};

// Test that everything gets parsed correctly, notifications are sent,
// and CanShow() is handled correctly under variety of conditions.
TEST_F(NotificationPromoTest, NotificationPromoJSONTest) {
  Init(
      "{"
      "  \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "  \"end\":\"$1\","
      "  \"promo_text\":\"What do you think of Chrome?\","
      "  \"payload\":"
      "    {"
      "      \"days_active\":7,"
      "      \"install_age_days\":21"
      "    },"
      "  \"max_views\":30,"
      "  \"max_seconds\":30,"
      "  \"promo_id\":0"
      "}",
      "What do you think of Chrome?",
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      0, 30, 30);

  InitPromoFromJson();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();

  TestFirstViewTimeRecorded();
}

TEST_F(NotificationPromoTest, NotificationPromoFinchTest) {
  Init(
      "{"
      "  \"start\":\"3 Aug 1999 9:26:06 GMT\","
      "  \"end\":\"$1\","
      "  \"promo_text\":\"What do you think of Chrome?\","
      "  \"payload\":"
      "    {"
      "      \"days_active\":7,"
      "      \"install_age_days\":21"
      "    },"
      "  \"max_views\":30,"
      "  \"max_seconds\":30,"
      "  \"promo_id\":0"
      "}",
      "What do you think of Chrome?",
      933672366,  // unix epoch for 3 Aug 1999 9:26:06 GMT.
      0, 30, 30);

  InitPromoFromVariations();

  // Test various conditions of CanShow.
  TestViews();
  TestClosed();
  TestPromoText();
  TestTime();
  TestMaxTime();

  TestFirstViewTimeRecorded();
}

}  // namespace ios
