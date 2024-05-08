// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/filter_factory.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/predictor_factory.h"
#include "ui/base/prediction/empty_filter.h"
#include "ui/base/prediction/empty_predictor.h"
#include "ui/base/prediction/kalman_predictor.h"
#include "ui/base/prediction/least_squares_predictor.h"
#include "ui/base/prediction/linear_predictor.h"
#include "ui/base/ui_base_features.h"

namespace blink {
namespace test {
namespace {

constexpr double kEpsilon = 0.001;

}  // namespace

class ScrollPredictorTest : public testing::Test {
 public:
  ScrollPredictorTest() {}
  ScrollPredictorTest(const ScrollPredictorTest&) = delete;
  ScrollPredictorTest& operator=(const ScrollPredictorTest&) = delete;

  void SetUp() override {
    original_events_.clear();
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
    scroll_predictor_->predictor_ = std::make_unique<ui::EmptyPredictor>();
  }

  void SetUpLSQPredictor() {
    scroll_predictor_->predictor_ =
        std::make_unique<ui::LeastSquaresPredictor>();
  }

  std::unique_ptr<WebInputEvent> CreateGestureScrollUpdate(
      float delta_x = 0,
      float delta_y = 0,
      double time_delta_in_milliseconds = 0,
      WebGestureEvent::InertialPhaseState phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum) {
    auto gesture = std::make_unique<WebGestureEvent>(
        WebInputEvent::Type::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests() +
            base::Milliseconds(time_delta_in_milliseconds),
        WebGestureDevice::kTouchscreen);
    gesture->data.scroll_update.delta_x = delta_x;
    gesture->data.scroll_update.delta_y = delta_y;
    gesture->data.scroll_update.inertial_phase = phase;

    original_events_.emplace_back(std::make_unique<WebCoalescedInputEvent>(
                                      gesture->Clone(), ui::LatencyInfo()),
                                  nullptr, base::NullCallback());

    return gesture;
  }

  void CoalesceWith(const std::unique_ptr<WebInputEvent>& new_event,
                    std::unique_ptr<WebInputEvent>& old_event) {
    old_event->Coalesce(*new_event);
  }

  void SendGestureScrollBegin() {
    WebGestureEvent gesture_begin(WebInputEvent::Type::kGestureScrollBegin,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests(),
                                  WebGestureDevice::kTouchscreen);
    scroll_predictor_->ResetOnGestureScrollBegin(gesture_begin);
  }

  void HandleResampleScrollEvents(std::unique_ptr<WebInputEvent>& event,
                                  double time_delta_in_milliseconds = 0,
                                  double display_refresh_rate = 30) {
    auto event_with_callback = std::make_unique<EventWithCallback>(
        std::make_unique<WebCoalescedInputEvent>(std::move(event),
                                                 ui::LatencyInfo()),
        base::NullCallback(), nullptr);
    event_with_callback->original_events() = std::move(original_events_);

    base::TimeDelta frame_interval = base::Seconds(1.0f / display_refresh_rate);
    event_with_callback = scroll_predictor_->ResampleScrollEvents(
        std::move(event_with_callback),
        WebInputEvent::GetStaticTimeStampForTests() +
            base::Milliseconds(time_delta_in_milliseconds),
        frame_interval);

    event = event_with_callback->event().Clone();
  }

  std::unique_ptr<ui::InputPredictor::InputData> PredictionAvailable(
      double time_delta_in_milliseconds = 0) {
    base::TimeTicks frame_time = WebInputEvent::GetStaticTimeStampForTests() +
                                 base::Milliseconds(time_delta_in_milliseconds);
    // Tests with 60Hz.
    return scroll_predictor_->predictor_->GeneratePrediction(frame_time);
  }

  gfx::PointF GetLastAccumulatedDelta() {
    return scroll_predictor_->last_predicted_accumulated_delta_;
  }

  bool GetResamplingState() {
    return scroll_predictor_->should_resample_scroll_events_;
  }

  bool isFilteringEnabled() { return scroll_predictor_->filtering_enabled_; }

  void ConfigurePredictorFieldTrialAndInitialize(
      const base::Feature& feature,
      const std::string& predictor_type) {
    ConfigurePredictorAndFilterInternal(
        feature, predictor_type, /* enable_filtering = */ false,
        blink::features::kFilteringScrollPrediction, "");
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
  }

  void ConfigureFilterFieldTrialAndInitialize(const base::Feature& feature,
                                              const std::string& filter_name) {
    // We still need the resampler feature to construct the scroll predictor at
    // all but just initialize it to defaults.
    ConfigurePredictorAndFilterInternal(
        blink::features::kResamplingScrollEvents, "",
        /* enable_filtering = */ true, feature, filter_name);
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
  }

  void ConfigurePredictorAndFilterFieldTrialAndInitialize(
      const base::Feature& pred_feature,
      const std::string& predictor_type,
      const base::Feature& filter_feature,
      const std::string& filter_type) {
    ConfigurePredictorAndFilterInternal(pred_feature, predictor_type,
                                        /* enable_filtering = */ true,
                                        filter_feature, filter_type);
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
  }

  // Helper method to set up both related features so tests have a consistent
  // view of the world. We assume that the predictor is always enabled (for the
  // scroll_predictor_unittests), but filter could be enabled or disabled.
  void ConfigurePredictorAndFilterInternal(const base::Feature& pred_feature,
                                           const std::string& predictor_type,
                                           bool enable_filtering,
                                           const base::Feature& filter_feature,
                                           const std::string& filter_type) {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;

    base::FieldTrialParams pred_field_params;
    pred_field_params["predictor"] = predictor_type;
    base::test::FeatureRefAndParams prediction_params = {pred_feature,
                                                         pred_field_params};

    base::FieldTrialParams filter_field_params;
    filter_field_params["filter"] = filter_type;
    base::test::FeatureRefAndParams filter_params = {filter_feature,
                                                     filter_field_params};

    enabled.emplace_back(
        base::test::FeatureRefAndParams(pred_feature, pred_field_params));
    if (enable_filtering) {
      enabled.emplace_back(
          base::test::FeatureRefAndParams(filter_feature, filter_field_params));
    } else {
      disabled.emplace_back(base::test::FeatureRef(filter_feature));
    }

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);

    EXPECT_EQ(pred_field_params["predictor"],
              GetFieldTrialParamValueByFeature(pred_feature, "predictor"));
    if (enable_filtering) {
      EXPECT_EQ(filter_field_params["filter"],
                GetFieldTrialParamValueByFeature(filter_feature, "filter"));
    }
  }

  void VerifyPredictorType(const char* expected_type) {
    EXPECT_EQ(expected_type, scroll_predictor_->predictor_->GetName());
  }

  void VerifyFilterType(const char* expected_type) {
    EXPECT_EQ(expected_type, scroll_predictor_->filter_->GetName());
  }

  void InitLinearResamplingTest(bool use_frames_based_experimental_prediction) {
    base::FieldTrialParams predictor_params;
    predictor_params["predictor"] = ::features::kPredictorNameLinearResampling;
    base::test::FeatureRefAndParams prediction_params = {
        features::kResamplingScrollEvents, predictor_params};

    base::FieldTrialParams prediction_type_params;
    prediction_type_params["mode"] =
        use_frames_based_experimental_prediction
            ? ::features::kPredictionTypeFramesBased
            : ::features::kPredictionTypeTimeBased;
    base::test::FeatureRefAndParams experimental_prediction_params = {
        ::features::kResamplingScrollEventsExperimentalPrediction,
        prediction_type_params};

    base::FieldTrialParams filter_params;
    filter_params["filter"] = "";
    base::test::FeatureRefAndParams resampling_and_filter = {
        features::kFilteringScrollPrediction, filter_params};

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {prediction_params, experimental_prediction_params,
         resampling_and_filter},
        {});
    scroll_predictor_ = std::make_unique<ScrollPredictor>();

    VerifyPredictorType(::features::kPredictorNameLinearResampling);
  }

 protected:
  EventWithCallback::OriginalEventList original_events_;
  std::unique_ptr<ScrollPredictor> scroll_predictor_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ScrollPredictorTest, ScrollResamplingStates) {
  // initial
  EXPECT_FALSE(GetResamplingState());

  // after GSB
  SendGestureScrollBegin();
  EXPECT_TRUE(GetResamplingState());

  // after GSU with no phase
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  EXPECT_TRUE(GetResamplingState());

  // after GSU with momentum phase
  gesture_update = CreateGestureScrollUpdate(
      0, 10, 10 /* ms */, WebGestureEvent::InertialPhaseState::kMomentum);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  EXPECT_FALSE(GetResamplingState());

  // after GSE
  std::unique_ptr<WebInputEvent> gesture_end =
      std::make_unique<WebGestureEvent>(
          WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
          WebInputEvent::GetStaticTimeStampForTests(),
          WebGestureDevice::kTouchscreen);
  HandleResampleScrollEvents(gesture_end);
  EXPECT_FALSE(GetResamplingState());
}

TEST_F(ScrollPredictorTest, ResampleGestureScrollEvents) {
  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameEmpty);
  SendGestureScrollBegin();
  EXPECT_FALSE(PredictionAvailable());

  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);

  // Aggregated event delta doesn't change with empty predictor applied.
  gesture_update = CreateGestureScrollUpdate(0, -20);
  CoalesceWith(CreateGestureScrollUpdate(0, -40), gesture_update);
  EXPECT_EQ(-60, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-60, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);

  // Cumulative amount of scroll from the GSB is stored in the empty predictor.
  auto result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(-80, result->pos.y());

  // Send another GSB, Prediction will be reset.
  SendGestureScrollBegin();
  EXPECT_FALSE(PredictionAvailable());

  // Sent another GSU.
  gesture_update = CreateGestureScrollUpdate(0, -35);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-35, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  // Total amount of scroll is track from the last GSB.
  result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(-35, result->pos.y());
}

TEST_F(ScrollPredictorTest, ScrollInDifferentDirection) {
  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameEmpty);
  SendGestureScrollBegin();

  // Scroll down.
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  auto result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(-20, result->pos.y());

  // Scroll up.
  gesture_update = CreateGestureScrollUpdate(0, 25);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(0, static_cast<const WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_x);
  EXPECT_EQ(25, static_cast<const WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_y);
  result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(0, result->pos.x());
  EXPECT_EQ(5, result->pos.y());

  // Scroll left + right.
  gesture_update = CreateGestureScrollUpdate(-35, 0);
  CoalesceWith(CreateGestureScrollUpdate(60, 0), gesture_update);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(25, static_cast<const WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_x);
  EXPECT_EQ(0, static_cast<const WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_y);
  result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(25, result->pos.x());
  EXPECT_EQ(5, result->pos.y());
}

TEST_F(ScrollPredictorTest, ScrollUpdateWithEmptyOriginalEventList) {
  SendGestureScrollBegin();

  // Send a GSU with empty original event list.
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, -20);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);

  // No prediction available because the event is skipped.
  EXPECT_FALSE(PredictionAvailable());

  // Send a GSU with original event.
  gesture_update = CreateGestureScrollUpdate(0, -30);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-30, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);

  // Send another GSU with empty original event list.
  gesture_update = CreateGestureScrollUpdate(0, -40);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-40, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);

  // Prediction only track GSU with original event list.
  auto result = PredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_EQ(-30, result->pos.y());
}

TEST_F(ScrollPredictorTest, LSQPredictorTest) {
  ConfigureFilterFieldTrialAndInitialize(features::kFilteringScrollPrediction,
                                         "");
  SetUpLSQPredictor();
  SendGestureScrollBegin();

  // Send 1st GSU, no prediction available.
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, -30, 8 /* ms */);
  HandleResampleScrollEvents(gesture_update, 16 /* ms */);
  EXPECT_FALSE(PredictionAvailable(16 /* ms */));
  EXPECT_EQ(-30, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(8 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());

  // Send 2nd GSU, no prediction available, event aligned at original timestamp.
  gesture_update = CreateGestureScrollUpdate(0, -30, 16 /* ms */);
  HandleResampleScrollEvents(gesture_update, 24 /* ms */);
  EXPECT_EQ(-30, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(16 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());
  EXPECT_FALSE(PredictionAvailable(24 /* ms */));

  // Send 3rd and 4th GSU, prediction result returns the sum of delta_y, event
  // aligned at frame time.
  gesture_update = CreateGestureScrollUpdate(0, -30, 24 /* ms */);
  HandleResampleScrollEvents(gesture_update, 32 /* ms */);
  EXPECT_EQ(-60, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(32 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());
  auto result = PredictionAvailable(32 /* ms */);
  EXPECT_TRUE(result);
  EXPECT_EQ(-120, result->pos.y());

  gesture_update = CreateGestureScrollUpdate(0, -30, 32 /* ms */);
  HandleResampleScrollEvents(gesture_update, 40 /* ms */);
  EXPECT_EQ(-30, static_cast<const WebGestureEvent*>(gesture_update.get())
                     ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(40 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());
  result = PredictionAvailable(40 /* ms */);
  EXPECT_TRUE(result);
  EXPECT_EQ(-150, result->pos.y());
}

TEST_F(ScrollPredictorTest, LinearResamplingPredictorTest) {
  // Test kResamplingScrollEventsExperimentalLatencyFixed
  InitLinearResamplingTest(false);
  SendGestureScrollBegin();

  // Send 1st GSU, no prediction available.
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 10 /* ms */, 30 /* Hz */);
  EXPECT_EQ(10, static_cast<const WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(10 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());

  // Prediction using fixed +3.3ms latency.
  gesture_update = CreateGestureScrollUpdate(0, 10, 20 /* ms */);
  HandleResampleScrollEvents(gesture_update, 20 /* ms */, 30 /* Hz */);
  ASSERT_FLOAT_EQ(10 + 3.3,
                  static_cast<const WebGestureEvent*>(gesture_update.get())
                      ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(23.3 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());

  // Test kResamplingScrollEventsExperimentalLatencyVariable
  InitLinearResamplingTest(true);
  SendGestureScrollBegin();

  // Send 1st GSU, no prediction available.
  gesture_update = CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 10 /* ms */, 60 /* Hz */);
  EXPECT_EQ(10, static_cast<const WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(10 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());

  // Prediction at 60Hz: uses experimental latency of 0.5 * 1/60 seconds.
  // Remember linear resampling has its -5 built-in latency.
  gesture_update = CreateGestureScrollUpdate(0, 10, 20 /* ms */);
  HandleResampleScrollEvents(gesture_update, 20 /* ms */, 60 /* Hz */);
  ASSERT_FLOAT_EQ(10 - 5 + 8.333,
                  static_cast<const WebGestureEvent*>(gesture_update.get())
                      ->data.scroll_update.delta_y);
  EXPECT_EQ(
      WebInputEvent::GetStaticTimeStampForTests() +
          base::Milliseconds(10 + 10 - 5 + 8.333 /* ms */),
      static_cast<const WebGestureEvent*>(gesture_update.get())->TimeStamp());
}

TEST_F(ScrollPredictorTest, ScrollPredictorNotChangeScrollDirection) {
  ConfigureFilterFieldTrialAndInitialize(features::kFilteringScrollPrediction,
                                         "");
  SetUpLSQPredictor();
  SendGestureScrollBegin();

  // Send 4 GSUs with delta_y = 10
  std::unique_ptr<WebInputEvent> gesture_update =
      CreateGestureScrollUpdate(0, 10, 10 /* ms */);
  HandleResampleScrollEvents(gesture_update, 15 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 20 /* ms */);
  HandleResampleScrollEvents(gesture_update, 25 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 30 /* ms */);
  HandleResampleScrollEvents(gesture_update, 35 /* ms */);
  gesture_update = CreateGestureScrollUpdate(0, 10, 40 /* ms */);
  HandleResampleScrollEvents(gesture_update, 45 /* ms */);
  EXPECT_NEAR(10,
              static_cast<const WebGestureEvent*>(gesture_update.get())
                  ->data.scroll_update.delta_y,
              kEpsilon);
  EXPECT_NEAR(45, GetLastAccumulatedDelta().y(), kEpsilon);

  // Send a GSU with delta_y = 2. So last resampled GSU we calculated is
  // overhead. No scroll back in this case.
  gesture_update = CreateGestureScrollUpdate(0, 2, 50 /* ms */);
  HandleResampleScrollEvents(gesture_update, 55 /* ms */);
  EXPECT_EQ(0, static_cast<const WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_y);
  EXPECT_NEAR(45, GetLastAccumulatedDelta().y(), kEpsilon);

  // Send a GSU with different scroll direction. Resampled GSU is in the new
  // direction.
  gesture_update = CreateGestureScrollUpdate(0, -6, 60 /* ms */);
  HandleResampleScrollEvents(gesture_update, 60 /* ms */);
  EXPECT_NEAR(-9,
              static_cast<const WebGestureEvent*>(gesture_update.get())
                  ->data.scroll_update.delta_y,
              kEpsilon);
  EXPECT_NEAR(36, GetLastAccumulatedDelta().y(), kEpsilon);
}

TEST_F(ScrollPredictorTest, ScrollPredictorTypeSelection) {
  // Use LinearResampling predictor by default.
  scroll_predictor_ = std::make_unique<ScrollPredictor>();
  VerifyPredictorType(::features::kPredictorNameLinearResampling);

  // When resampling is enabled, predictor type is set from
  // kResamplingScrollEvents.
  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameEmpty);
  VerifyPredictorType(::features::kPredictorNameEmpty);

  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameLsq);
  VerifyPredictorType(::features::kPredictorNameLsq);

  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameKalman);
  VerifyPredictorType(::features::kPredictorNameKalman);

  ConfigurePredictorFieldTrialAndInitialize(
      features::kResamplingScrollEvents, ::features::kPredictorNameLinearFirst);
  VerifyPredictorType(::features::kPredictorNameLinearFirst);
}

// Check the right filter is selected
TEST_F(ScrollPredictorTest, DefaultFilter) {
  ConfigureFilterFieldTrialAndInitialize(features::kFilteringScrollPrediction,
                                         "");
  VerifyFilterType(::features::kFilterNameEmpty);
  EXPECT_TRUE(isFilteringEnabled());

  ConfigureFilterFieldTrialAndInitialize(features::kFilteringScrollPrediction,
                                         ::features::kFilterNameEmpty);
  VerifyFilterType(::features::kFilterNameEmpty);
  EXPECT_TRUE(isFilteringEnabled());

  ConfigureFilterFieldTrialAndInitialize(features::kFilteringScrollPrediction,
                                         ::features::kFilterNameOneEuro);
  VerifyFilterType(::features::kFilterNameOneEuro);
  EXPECT_TRUE(isFilteringEnabled());
}

// We first send 100 events to the scroll predictor with kalman predictor
// enabled and filtering disabled and save the results.
// We then send the same events with kalman and the empty filter, we should
// expect the same results.
TEST_F(ScrollPredictorTest, FilteringPrediction) {
  ConfigurePredictorFieldTrialAndInitialize(features::kResamplingScrollEvents,
                                            ::features::kPredictorNameKalman);

  std::vector<double> accumulated_deltas;
  std::unique_ptr<WebInputEvent> gesture_update;

  for (int i = 0; i < 100; i++) {
    // Create event at time 8*i
    gesture_update = CreateGestureScrollUpdate(0, 3 * i, 8 * i /* ms */);
    // Handle the event 5 ms later
    HandleResampleScrollEvents(gesture_update, 8 * i + 5 /* ms */);
    EXPECT_FALSE(isFilteringEnabled());
    accumulated_deltas.push_back(GetLastAccumulatedDelta().y());
  }
  EXPECT_EQ((int)accumulated_deltas.size(), 100);

  // Now we enable filtering and compare the deltas
  ConfigurePredictorAndFilterFieldTrialAndInitialize(
      features::kResamplingScrollEvents, ::features::kPredictorNameKalman,
      features::kFilteringScrollPrediction, ::features::kFilterNameEmpty);
  scroll_predictor_ = std::make_unique<ScrollPredictor>();

  for (int i = 0; i < 100; i++) {
    // Create event at time 8*i
    gesture_update = CreateGestureScrollUpdate(0, 3 * i, 8 * i /* ms */);
    // Handle the event 5 ms later
    HandleResampleScrollEvents(gesture_update, 8 * i + 5 /* ms */);
    EXPECT_TRUE(isFilteringEnabled());
    EXPECT_NEAR(accumulated_deltas[i], GetLastAccumulatedDelta().y(), 0.00001);
  }
}

}  // namespace test
}  // namespace blink
