// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_point_2d_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_settings_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainpoint2dparameters_point2dsequence.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_video_capturer_source.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using ExpectHasExposureModeAndFocusMode =
    base::StrongAlias<class ExpectHasExposureModeAndFocusModeTag, bool>;
using ExpectHasPanTiltZoom =
    base::StrongAlias<class ExpectHasPanTiltZoomTag, bool>;
using PopulatePanTiltZoom =
    base::StrongAlias<class PopulatePanTiltZoomZoomTag, bool>;

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

constexpr double kExposureCompensationDelta = 1;
constexpr double kExposureTimeDelta = 2;
constexpr double kColorTemperatureDelta = 3;
constexpr double kIsoDelta = 4;
constexpr double kBrightnessDelta = 5;
constexpr double kContrastDelta = 6;
constexpr double kSaturationDelta = 7;
constexpr double kSharpnessDelta = 8;
constexpr double kFocusDistanceDelta = 9;
constexpr double kPanDelta = 10;
constexpr double kTiltDelta = 11;
constexpr double kZoomDelta = 12;

// CaptureErrorFunction implements a javascript function which captures
// name, message and constraint of the exception passed as its argument.
class CaptureErrorFunction final : public ScriptFunction::Callable {
 public:
  CaptureErrorFunction() = default;

  bool WasCalled() const { return was_called_; }
  const String& Name() const { return name_; }
  const String& Message() const { return message_; }
  const String& Constraint() const { return constraint_; }

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    was_called_ = true;

    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();

    v8::Local<v8::Object> error_object =
        value.V8Value()->ToObject(context).ToLocalChecked();

    v8::Local<v8::Value> name =
        error_object->Get(context, V8String(isolate, "name")).ToLocalChecked();
    name_ = ToCoreString(isolate, name->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();
    message_ =
        ToCoreString(isolate, message->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> constraint =
        error_object->Get(context, V8String(isolate, "constraint"))
            .ToLocalChecked();
    constraint_ =
        ToCoreString(isolate, constraint->ToString(context).ToLocalChecked());

    return ScriptValue();
  }

 private:
  bool was_called_ = false;
  String name_;
  String message_;
  String constraint_;
};

// These traits and type aliases simplify mapping from bare value types (bool,
// double, sequence, string) to constrain value dictionary types.
template <typename T>
struct ConstrainWithDictionaryTraits;

template <>
struct ConstrainWithDictionaryTraits<bool> {
  using DictionaryType = ConstrainBooleanParameters;
};

template <>
struct ConstrainWithDictionaryTraits<double> {
  using DictionaryType = ConstrainDoubleRange;
};

template <>
struct ConstrainWithDictionaryTraits<HeapVector<Member<Point2D>>> {
  using DictionaryType = ConstrainPoint2DParameters;
};

template <>
struct ConstrainWithDictionaryTraits<String> {
  using DictionaryType = ConstrainDOMStringParameters;
};

template <>
struct ConstrainWithDictionaryTraits<Vector<String>> {
  using DictionaryType = ConstrainDOMStringParameters;
};

template <typename T>
using ConstrainWithDictionaryType =
    ConstrainWithDictionaryTraits<T>::DictionaryType;

// The `ConstrainDOMStringParameters` dictionary type has `exact` and `ideal`
// members of type `(DOMString or sequence<DOMString>)`.
// https://w3c.github.io/mediacapture-main/#dom-constraindomstringparameters
V8UnionStringOrStringSequence* CreateDictionaryMemberValue(
    const String& value) {
  return MakeGarbageCollected<V8UnionStringOrStringSequence>(value);
}

V8UnionStringOrStringSequence* CreateDictionaryMemberValue(
    const Vector<String>& value) {
  return MakeGarbageCollected<V8UnionStringOrStringSequence>(value);
}

// All the other constrain value dictionary types (ConstrainBooleanParameters,
// ConstrainDoubleRange and ConstrainPoint2DParameters) have `exact` and
// `ideal` members of non-union types.
// https://w3c.github.io/mediacapture-main/#dom-constrainbooleanparameters
// https://w3c.github.io/mediacapture-main/#dom-constraindoublerange
// https://w3c.github.io/mediacapture-main/#dom-constrainpoint2dparameters
template <typename T>
const T& CreateDictionaryMemberValue(const T& value) {
  return value;
}

// This creator creates bare value (bool, double, sequence, string)
// constraints.
struct ConstrainWithBareValueCreator {
  template <typename T>
  static const T& Create(const T& bare_value) {
    return bare_value;
  }
};

// This creator creates constrain value dictionary constraints without members.
struct ConstrainWithEmptyDictionaryCreator {
  template <typename T>
  static auto* Create(const T&) {
    return ConstrainWithDictionaryType<T>::Create();
  }
};

// This creator creates constrain value dictionary constraints with `exact`
// members.
struct ConstrainWithExactDictionaryCreator {
  template <typename T>
  static auto* Create(const T& exact) {
    auto* constrain_value = ConstrainWithDictionaryType<T>::Create();
    constrain_value->setExact(CreateDictionaryMemberValue(exact));
    return constrain_value;
  }
};

// This creator creates constrain value dictionary constraints with `ideal`
// members.
struct ConstrainWithIdealDictionaryCreator {
  template <typename T>
  static auto* Create(const T& ideal) {
    auto* constrain_value = ConstrainWithDictionaryType<T>::Create();
    constrain_value->setIdeal(CreateDictionaryMemberValue(ideal));
    return constrain_value;
  }
};

// This creator creates constrain value dictionary constraints with `max`
// members.
struct ConstrainWithMaxDictionaryCreator {
  template <typename T>
  static auto* Create(const T& max) {
    auto* constrain_value = ConstrainWithDictionaryType<T>::Create();
    constrain_value->setMax(max);
    return constrain_value;
  }
};

// This creator creates constrain value dictionary constraints with `max`
// members for numeric (double) values and empty constrain value dictionary
// constraints for non-numeric (bool, sequence, string) values.
struct ConstrainWithMaxOrEmptyDictionaryCreator {
  static auto* Create(double max) {
    return ConstrainWithMaxDictionaryCreator::Create(max);
  }

  template <typename T>
  static auto* Create(const T&) {
    return ConstrainWithDictionaryType<T>::Create();
  }
};

// This creator creates constrain value dictionary constraints with `min`
// members.
struct ConstrainWithMinDictionaryCreator {
  template <typename T>
  static auto* Create(const T& min) {
    auto* constrain_value = ConstrainWithDictionaryType<T>::Create();
    constrain_value->setMin(min);
    return constrain_value;
  }
};

// This creator creates constrain value dictionary constraints with `min`
// members for numeric (double) values and empty constrain value dictionary
// constraints for non-numeric (bool, sequence, string) values.
struct ConstrainWithMinOrEmptyDictionaryCreator {
  static auto* Create(double min) {
    return ConstrainWithMinDictionaryCreator::Create(min);
  }

  template <typename T>
  static auto* Create(const T&) {
    return ConstrainWithDictionaryType<T>::Create();
  }
};

MediaSettingsRange* CreateMediaSettingsRange(double min,
                                             double max,
                                             double step) {
  auto* range = MediaSettingsRange::Create();
  range->setMin(min);
  range->setMax(max);
  range->setStep(step);
  return range;
}

MediaSettingsRange* CreateMediaSettingsRange(const char (&str)[3]) {
  int min = (static_cast<int>(str[0]) << 16) + (static_cast<int>(str[1]) << 8);
  return CreateMediaSettingsRange(min, min + 16, 8);
}

Point2D* CreatePoint2D(double x, double y) {
  auto* point = Point2D::Create();
  point->setX(x);
  point->setY(y);
  return point;
}

double RangeMean(const MediaSettingsRange* range) {
  return (range->min() + range->max()) / 2;
}

void CheckExactValues(
    const media::mojom::blink::PhotoSettingsPtr& settings,
    const MediaTrackCapabilities* all_capabilities,
    ExpectHasPanTiltZoom expect_has_pan_tilt_zoom = ExpectHasPanTiltZoom(true),
    ExpectHasExposureModeAndFocusMode expect_has_exposure_mode_and_focus_mode =
        ExpectHasExposureModeAndFocusMode(true)) {
  EXPECT_TRUE(settings->has_white_balance_mode);
  EXPECT_EQ(settings->white_balance_mode,
            media::mojom::blink::MeteringMode::CONTINUOUS);
  if (expect_has_exposure_mode_and_focus_mode) {
    EXPECT_TRUE(settings->has_exposure_mode);
    EXPECT_EQ(settings->exposure_mode,
              media::mojom::blink::MeteringMode::MANUAL);
    EXPECT_TRUE(settings->has_focus_mode);
    EXPECT_EQ(settings->focus_mode, media::mojom::blink::MeteringMode::NONE);
  } else {
    EXPECT_FALSE(settings->has_exposure_mode);
    EXPECT_FALSE(settings->has_focus_mode);
  }
  ASSERT_EQ(settings->points_of_interest.size(), 3u);
  EXPECT_EQ(settings->points_of_interest[0]->x, 0.0);
  EXPECT_EQ(settings->points_of_interest[0]->y, 0.0);
  EXPECT_EQ(settings->points_of_interest[1]->x, 0.25);
  EXPECT_EQ(settings->points_of_interest[1]->y, 0.75);
  EXPECT_EQ(settings->points_of_interest[2]->x, 1.0);
  EXPECT_EQ(settings->points_of_interest[2]->y, 1.0);
  EXPECT_TRUE(settings->has_exposure_compensation);
  EXPECT_EQ(settings->exposure_compensation,
            all_capabilities->exposureCompensation()->min() +
                kExposureCompensationDelta);
  EXPECT_TRUE(settings->has_exposure_time);
  EXPECT_EQ(settings->exposure_time,
            all_capabilities->exposureTime()->min() + kExposureTimeDelta);
  EXPECT_TRUE(settings->has_color_temperature);
  EXPECT_EQ(
      settings->color_temperature,
      all_capabilities->colorTemperature()->min() + kColorTemperatureDelta);
  EXPECT_TRUE(settings->has_iso);
  EXPECT_EQ(settings->iso, all_capabilities->iso()->min() + kIsoDelta);
  EXPECT_TRUE(settings->has_brightness);
  EXPECT_EQ(settings->brightness,
            all_capabilities->brightness()->min() + kBrightnessDelta);
  EXPECT_TRUE(settings->has_contrast);
  EXPECT_EQ(settings->contrast,
            all_capabilities->contrast()->min() + kContrastDelta);
  EXPECT_TRUE(settings->has_saturation);
  EXPECT_EQ(settings->saturation,
            all_capabilities->saturation()->min() + kSaturationDelta);
  EXPECT_TRUE(settings->has_sharpness);
  EXPECT_EQ(settings->sharpness,
            all_capabilities->sharpness()->min() + kSharpnessDelta);
  EXPECT_TRUE(settings->has_focus_distance);
  EXPECT_EQ(settings->focus_distance,
            all_capabilities->focusDistance()->min() + kFocusDistanceDelta);
  if (expect_has_pan_tilt_zoom) {
    EXPECT_TRUE(settings->has_pan);
    EXPECT_EQ(settings->pan, all_capabilities->pan()->min() + kPanDelta);
    EXPECT_TRUE(settings->has_tilt);
    EXPECT_EQ(settings->tilt, all_capabilities->tilt()->min() + kTiltDelta);
    EXPECT_TRUE(settings->has_zoom);
    EXPECT_EQ(settings->zoom, all_capabilities->zoom()->min() + kZoomDelta);
  } else {
    EXPECT_FALSE(settings->has_pan);
    EXPECT_FALSE(settings->has_tilt);
    EXPECT_FALSE(settings->has_zoom);
  }
  EXPECT_TRUE(settings->has_torch);
  EXPECT_EQ(settings->torch, true);
  EXPECT_TRUE(settings->has_background_blur_mode);
  EXPECT_EQ(settings->background_blur_mode,
            media::mojom::blink::BackgroundBlurMode::BLUR);
  EXPECT_TRUE(settings->eye_gaze_correction_mode.has_value());
  EXPECT_EQ(settings->eye_gaze_correction_mode.value(),
            media::mojom::blink::EyeGazeCorrectionMode::OFF);
  EXPECT_TRUE(settings->has_face_framing_mode);
  EXPECT_EQ(settings->face_framing_mode,
            media::mojom::blink::MeteringMode::CONTINUOUS);
  EXPECT_TRUE(settings->background_segmentation_mask_state.has_value());
  EXPECT_FALSE(settings->background_segmentation_mask_state.value());
}

void CheckMaxValues(const media::mojom::blink::PhotoSettingsPtr& settings,
                    const MediaTrackCapabilities* all_capabilities,
                    const MediaTrackSettings* default_settings,
                    ExpectHasPanTiltZoom expect_has_pan_tilt_zoom =
                        ExpectHasPanTiltZoom(true)) {
  EXPECT_FALSE(settings->has_white_balance_mode);
  EXPECT_FALSE(settings->has_exposure_mode);
  EXPECT_FALSE(settings->has_focus_mode);
  EXPECT_EQ(settings->points_of_interest.size(), 0u);
  EXPECT_TRUE(settings->has_exposure_compensation);
  EXPECT_EQ(settings->exposure_compensation,
            std::min(all_capabilities->exposureCompensation()->min() +
                         kExposureCompensationDelta,
                     default_settings->exposureCompensation()));
  EXPECT_TRUE(settings->has_exposure_time);
  EXPECT_EQ(
      settings->exposure_time,
      std::min(all_capabilities->exposureTime()->min() + kExposureTimeDelta,
               default_settings->exposureTime()));
  EXPECT_TRUE(settings->has_color_temperature);
  EXPECT_EQ(settings->color_temperature,
            std::min(all_capabilities->colorTemperature()->min() +
                         kColorTemperatureDelta,
                     default_settings->colorTemperature()));
  EXPECT_TRUE(settings->has_iso);
  EXPECT_EQ(settings->iso, std::min(all_capabilities->iso()->min() + kIsoDelta,
                                    default_settings->iso()));
  EXPECT_TRUE(settings->has_brightness);
  EXPECT_EQ(settings->brightness,
            std::min(all_capabilities->brightness()->min() + kBrightnessDelta,
                     default_settings->brightness()));
  EXPECT_TRUE(settings->has_contrast);
  EXPECT_EQ(settings->contrast,
            std::min(all_capabilities->contrast()->min() + kContrastDelta,
                     default_settings->contrast()));
  EXPECT_TRUE(settings->has_saturation);
  EXPECT_EQ(settings->saturation,
            std::min(all_capabilities->saturation()->min() + kSaturationDelta,
                     default_settings->saturation()));
  EXPECT_TRUE(settings->has_sharpness);
  EXPECT_EQ(settings->sharpness,
            std::min(all_capabilities->sharpness()->min() + kSharpnessDelta,
                     default_settings->sharpness()));
  EXPECT_TRUE(settings->has_focus_distance);
  EXPECT_EQ(
      settings->focus_distance,
      std::min(all_capabilities->focusDistance()->min() + kFocusDistanceDelta,
               default_settings->focusDistance()));
  if (expect_has_pan_tilt_zoom) {
    EXPECT_TRUE(settings->has_pan);
    EXPECT_EQ(settings->pan,
              std::min(all_capabilities->pan()->min() + kPanDelta,
                       default_settings->pan()));
    EXPECT_TRUE(settings->has_tilt);
    EXPECT_EQ(settings->tilt,
              std::min(all_capabilities->tilt()->min() + kTiltDelta,
                       default_settings->tilt()));
    EXPECT_TRUE(settings->has_zoom);
    EXPECT_EQ(settings->zoom,
              std::min(all_capabilities->zoom()->min() + kZoomDelta,
                       default_settings->zoom()));
  } else {
    EXPECT_FALSE(settings->has_pan);
    EXPECT_FALSE(settings->has_tilt);
    EXPECT_FALSE(settings->has_zoom);
  }
  EXPECT_FALSE(settings->has_torch);
  EXPECT_FALSE(settings->has_background_blur_mode);
  EXPECT_FALSE(settings->eye_gaze_correction_mode.has_value());
  EXPECT_FALSE(settings->has_face_framing_mode);
  EXPECT_FALSE(settings->background_segmentation_mask_state.has_value());
}

void CheckMinValues(const media::mojom::blink::PhotoSettingsPtr& settings,
                    const MediaTrackCapabilities* all_capabilities,
                    const MediaTrackSettings* default_settings,
                    ExpectHasPanTiltZoom expect_has_pan_tilt_zoom =
                        ExpectHasPanTiltZoom(true)) {
  EXPECT_FALSE(settings->has_white_balance_mode);
  EXPECT_FALSE(settings->has_exposure_mode);
  EXPECT_FALSE(settings->has_focus_mode);
  EXPECT_EQ(settings->points_of_interest.size(), 0u);
  EXPECT_TRUE(settings->has_exposure_compensation);
  EXPECT_EQ(settings->exposure_compensation,
            std::max(all_capabilities->exposureCompensation()->min() +
                         kExposureCompensationDelta,
                     default_settings->exposureCompensation()));
  EXPECT_TRUE(settings->has_exposure_time);
  EXPECT_EQ(
      settings->exposure_time,
      std::max(all_capabilities->exposureTime()->min() + kExposureTimeDelta,
               default_settings->exposureTime()));
  EXPECT_TRUE(settings->has_color_temperature);
  EXPECT_EQ(settings->color_temperature,
            std::max(all_capabilities->colorTemperature()->min() +
                         kColorTemperatureDelta,
                     default_settings->colorTemperature()));
  EXPECT_TRUE(settings->has_iso);
  EXPECT_EQ(settings->iso, std::max(all_capabilities->iso()->min() + kIsoDelta,
                                    default_settings->iso()));
  EXPECT_TRUE(settings->has_brightness);
  EXPECT_EQ(settings->brightness,
            std::max(all_capabilities->brightness()->min() + kBrightnessDelta,
                     default_settings->brightness()));
  EXPECT_TRUE(settings->has_contrast);
  EXPECT_EQ(settings->contrast,
            std::max(all_capabilities->contrast()->min() + kContrastDelta,
                     default_settings->contrast()));
  EXPECT_TRUE(settings->has_saturation);
  EXPECT_EQ(settings->saturation,
            std::max(all_capabilities->saturation()->min() + kSaturationDelta,
                     default_settings->saturation()));
  EXPECT_TRUE(settings->has_sharpness);
  EXPECT_EQ(settings->sharpness,
            std::max(all_capabilities->sharpness()->min() + kSharpnessDelta,
                     default_settings->sharpness()));
  EXPECT_TRUE(settings->has_focus_distance);
  EXPECT_EQ(
      settings->focus_distance,
      std::max(all_capabilities->focusDistance()->min() + kFocusDistanceDelta,
               default_settings->focusDistance()));
  if (expect_has_pan_tilt_zoom) {
    EXPECT_TRUE(settings->has_pan);
    EXPECT_EQ(settings->pan,
              std::max(all_capabilities->pan()->min() + kPanDelta,
                       default_settings->pan()));
    EXPECT_TRUE(settings->has_tilt);
    EXPECT_EQ(settings->tilt,
              std::max(all_capabilities->tilt()->min() + kTiltDelta,
                       default_settings->tilt()));
    EXPECT_TRUE(settings->has_zoom);
    EXPECT_EQ(settings->zoom,
              std::max(all_capabilities->zoom()->min() + kZoomDelta,
                       default_settings->zoom()));
  } else {
    EXPECT_FALSE(settings->has_pan);
    EXPECT_FALSE(settings->has_tilt);
    EXPECT_FALSE(settings->has_zoom);
  }
  EXPECT_FALSE(settings->has_torch);
  EXPECT_FALSE(settings->has_background_blur_mode);
  EXPECT_FALSE(settings->eye_gaze_correction_mode.has_value());
  EXPECT_FALSE(settings->has_face_framing_mode);
  EXPECT_FALSE(settings->background_segmentation_mask_state.has_value());
}

void CheckNoValues(const media::mojom::blink::PhotoSettingsPtr& settings,
                   size_t expected_points_of_interest_size = 0u) {
  EXPECT_FALSE(settings->has_white_balance_mode);
  EXPECT_FALSE(settings->has_exposure_mode);
  EXPECT_FALSE(settings->has_focus_mode);
  EXPECT_EQ(settings->points_of_interest.size(),
            expected_points_of_interest_size);
  EXPECT_FALSE(settings->has_exposure_compensation);
  EXPECT_FALSE(settings->has_exposure_time);
  EXPECT_FALSE(settings->has_color_temperature);
  EXPECT_FALSE(settings->has_iso);
  EXPECT_FALSE(settings->has_brightness);
  EXPECT_FALSE(settings->has_contrast);
  EXPECT_FALSE(settings->has_saturation);
  EXPECT_FALSE(settings->has_sharpness);
  EXPECT_FALSE(settings->has_focus_distance);
  EXPECT_FALSE(settings->has_pan);
  EXPECT_FALSE(settings->has_tilt);
  EXPECT_FALSE(settings->has_zoom);
  EXPECT_FALSE(settings->has_torch);
  EXPECT_FALSE(settings->has_background_blur_mode);
  EXPECT_FALSE(settings->eye_gaze_correction_mode.has_value());
  EXPECT_FALSE(settings->has_face_framing_mode);
  EXPECT_FALSE(settings->background_segmentation_mask_state.has_value());
}

template <typename ConstraintCreator>
void PopulateConstraintSet(
    MediaTrackConstraintSet* constraint_set,
    const MediaTrackCapabilities* all_capabilities,
    PopulatePanTiltZoom populate_pan_tilt_zoom = PopulatePanTiltZoom(true)) {
  constraint_set->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstraintCreator::Create(all_capabilities->whiteBalanceMode()[0])));
  constraint_set->setExposureMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstraintCreator::Create(all_capabilities->exposureMode())));
  constraint_set->setFocusMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstraintCreator::Create(all_capabilities->focusMode())));
  HeapVector<Member<Point2D>> points_of_interest = {CreatePoint2D(-0.75, -0.25),
                                                    CreatePoint2D(0.25, 0.75),
                                                    CreatePoint2D(1.25, 1.75)};
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          ConstraintCreator::Create(points_of_interest)));
  constraint_set->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(
              all_capabilities->exposureCompensation()->min() +
              kExposureCompensationDelta)));
  constraint_set->setExposureTime(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->exposureTime()->min() +
                                    kExposureTimeDelta)));
  constraint_set->setColorTemperature(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(
              all_capabilities->colorTemperature()->min() +
              kColorTemperatureDelta)));
  constraint_set->setIso(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->iso()->min() +
                                    kIsoDelta)));
  constraint_set->setBrightness(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->brightness()->min() +
                                    kBrightnessDelta)));
  constraint_set->setContrast(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->contrast()->min() +
                                    kContrastDelta)));
  constraint_set->setSaturation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->saturation()->min() +
                                    kSaturationDelta)));
  constraint_set->setSharpness(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->sharpness()->min() +
                                    kSharpnessDelta)));
  constraint_set->setFocusDistance(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstraintCreator::Create(all_capabilities->focusDistance()->min() +
                                    kFocusDistanceDelta)));
  if (populate_pan_tilt_zoom) {
    constraint_set->setPan(
        MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
            ConstraintCreator::Create(all_capabilities->pan()->min() +
                                      kPanDelta)));
    constraint_set->setTilt(
        MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
            ConstraintCreator::Create(all_capabilities->tilt()->min() +
                                      kTiltDelta)));
    constraint_set->setZoom(
        MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
            ConstraintCreator::Create(all_capabilities->zoom()->min() +
                                      kZoomDelta)));
  }
  constraint_set->setTorch(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstraintCreator::Create(true)));
  constraint_set->setBackgroundBlur(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstraintCreator::Create(all_capabilities->backgroundBlur()[0])));
  constraint_set->setEyeGazeCorrection(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstraintCreator::Create(all_capabilities->eyeGazeCorrection()[0])));
  constraint_set->setFaceFraming(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstraintCreator::Create(all_capabilities->faceFraming()[0])));
  constraint_set->setBackgroundSegmentationMask(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstraintCreator::Create(
              all_capabilities->backgroundSegmentationMask()[0])));
}

class MockMediaStreamComponent
    : public GarbageCollected<MockMediaStreamComponent>,
      public MediaStreamComponent {
 public:
  virtual ~MockMediaStreamComponent() = default;
  MOCK_CONST_METHOD0(Clone, MediaStreamComponent*());
  MOCK_CONST_METHOD0(Source, MediaStreamSource*());
  MOCK_CONST_METHOD0(Id, String());
  MOCK_CONST_METHOD0(UniqueId, int());
  MOCK_CONST_METHOD0(GetSourceType, MediaStreamSource::StreamType());
  MOCK_CONST_METHOD0(GetSourceName, const String&());
  MOCK_CONST_METHOD0(GetReadyState, MediaStreamSource::ReadyState());
  MOCK_CONST_METHOD0(Remote, bool());
  MOCK_CONST_METHOD0(Enabled, bool());
  MOCK_METHOD1(SetEnabled, void(bool));
  MOCK_METHOD0(ContentHint, WebMediaStreamTrack::ContentHintType());
  MOCK_METHOD1(SetContentHint, void(WebMediaStreamTrack::ContentHintType));
  MOCK_CONST_METHOD0(GetPlatformTrack, MediaStreamTrackPlatform*());
  MOCK_METHOD1(SetPlatformTrack,
               void(std::unique_ptr<MediaStreamTrackPlatform>));
  MOCK_METHOD1(GetSettings, void(MediaStreamTrackPlatform::Settings&));
  MOCK_METHOD0(GetCaptureHandle, MediaStreamTrackPlatform::CaptureHandle());
  MOCK_METHOD0(CreationFrame, WebLocalFrame*());
  MOCK_METHOD1(SetCreationFrameGetter,
               void(base::RepeatingCallback<WebLocalFrame*()>));
  MOCK_METHOD1(AddSourceObserver, void(MediaStreamSource::Observer*));
  MOCK_METHOD1(AddSink, void(WebMediaStreamAudioSink*));
  MOCK_METHOD4(AddSink,
               void(WebMediaStreamSink*,
                    const VideoCaptureDeliverFrameCB&,
                    MediaStreamVideoSink::IsSecure,
                    MediaStreamVideoSink::UsesAlpha));
  MOCK_CONST_METHOD0(ToString, String());
};

}  // namespace

class ImageCaptureTest : public testing::Test {
 public:
  ImageCaptureTest()
      : component_(MakeGarbageCollected<MockMediaStreamComponent>()),
        track_(MakeGarbageCollected<MockMediaStreamTrack>()),
        image_capture_(MakeGarbageCollected<ImageCapture>(
            /*execution_context=*/nullptr,
            track_,
            /*pan_tilt_zoom_allowed=*/true,
            base::DoNothing(),
            base::Milliseconds(1))) {
    track_->SetComponent(component_);
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

  void SetupTrackMocks(V8TestingScope& scope,
                       bool produce_frame_on_add_sink = true) {
    produce_frame_on_add_sink_ = produce_frame_on_add_sink;
    source_ = std::make_unique<MediaStreamVideoCapturerSource>(
        scope.GetFrame().GetTaskRunner(TaskType::kInternalMediaRealTime),
        &scope.GetFrame(),
        MediaStreamVideoCapturerSource::SourceStoppedCallback(),
        std::make_unique<NiceMock<MockVideoCapturerSource>>());
    platform_track_ = std::make_unique<MediaStreamVideoTrack>(
        source_.get(), WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        /*enabled=*/true);
    EXPECT_CALL(*component_, GetPlatformTrack)
        .WillRepeatedly(Return(platform_track_.get()));
    EXPECT_CALL(*component_, GetSourceType)
        .WillRepeatedly(Return(MediaStreamSource::kTypeVideo));

    ON_CALL(*component_, AddSink(_, _, _, _))
        .WillByDefault(Invoke([&](WebMediaStreamSink* sink,
                                  const VideoCaptureDeliverFrameCB& callback,
                                  MediaStreamVideoSink::IsSecure is_secure,
                                  MediaStreamVideoSink::UsesAlpha uses_alpha) {
          platform_track_->AddSink(sink, callback, is_secure, uses_alpha);
          if (produce_frame_on_add_sink_) {
            callback.Run(media::VideoFrame::CreateBlackFrame(gfx::Size(1, 1)),
                         /*estimated_capture_time=*/base::TimeTicks());
          }
        }));
  }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<MockMediaStreamComponent> component_;
  Persistent<MockMediaStreamTrack> track_;
  Persistent<ImageCapture> image_capture_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  std::unique_ptr<MediaStreamVideoCapturerSource> source_;
  std::unique_ptr<MediaStreamVideoTrack> platform_track_;
  bool produce_frame_on_add_sink_ = true;
};

class ImageCaptureConstraintTest : public ImageCaptureTest {
 public:
  ImageCaptureConstraintTest() {
    all_capabilities_ = MediaTrackCapabilities::Create();
    all_non_capabilities_ = MediaTrackCapabilities::Create();
    all_capabilities_->setWhiteBalanceMode({"continuous", "manual"});
    all_non_capabilities_->setWhiteBalanceMode({"none"});
    all_capabilities_->setExposureMode({"manual", "none"});
    all_non_capabilities_->setExposureMode({"continuous"});
    all_capabilities_->setFocusMode({"none", "continuous"});
    all_non_capabilities_->setFocusMode({"manual"});
    all_capabilities_->setExposureCompensation(CreateMediaSettingsRange("ec"));
    all_capabilities_->setExposureTime(CreateMediaSettingsRange("et"));
    all_capabilities_->setColorTemperature(CreateMediaSettingsRange("ct"));
    all_capabilities_->setIso(CreateMediaSettingsRange("is"));
    all_capabilities_->setBrightness(CreateMediaSettingsRange("br"));
    all_capabilities_->setContrast(CreateMediaSettingsRange("co"));
    all_capabilities_->setSaturation(CreateMediaSettingsRange("sa"));
    all_capabilities_->setSharpness(CreateMediaSettingsRange("sh"));
    all_capabilities_->setFocusDistance(CreateMediaSettingsRange("fd"));
    all_capabilities_->setPan(CreateMediaSettingsRange("pa"));
    all_capabilities_->setTilt(CreateMediaSettingsRange("ti"));
    all_capabilities_->setZoom(CreateMediaSettingsRange("zo"));
    all_capabilities_->setTorch(true);
    all_capabilities_->setBackgroundBlur({true});
    all_capabilities_->setEyeGazeCorrection({false});
    all_capabilities_->setFaceFraming({true, false});
    all_capabilities_->setBackgroundSegmentationMask({false, true});
    all_non_capabilities_->setBackgroundBlur({false});
    all_non_capabilities_->setEyeGazeCorrection({true});
    default_settings_ = MediaTrackSettings::Create();
    default_settings_->setWhiteBalanceMode(
        all_capabilities_->whiteBalanceMode()[0]);
    default_settings_->setExposureMode(all_capabilities_->exposureMode()[0]);
    default_settings_->setFocusMode(all_capabilities_->focusMode()[0]);
    default_settings_->setExposureCompensation(
        RangeMean(all_capabilities_->exposureCompensation()));
    default_settings_->setExposureTime(
        RangeMean(all_capabilities_->exposureTime()));
    default_settings_->setColorTemperature(
        RangeMean(all_capabilities_->colorTemperature()));
    default_settings_->setIso(RangeMean(all_capabilities_->iso()));
    default_settings_->setBrightness(
        RangeMean(all_capabilities_->brightness()));
    default_settings_->setContrast(RangeMean(all_capabilities_->contrast()));
    default_settings_->setSaturation(
        RangeMean(all_capabilities_->saturation()));
    default_settings_->setSharpness(RangeMean(all_capabilities_->sharpness()));
    default_settings_->setFocusDistance(
        RangeMean(all_capabilities_->focusDistance()));
    default_settings_->setPan(RangeMean(all_capabilities_->pan()));
    default_settings_->setTilt(RangeMean(all_capabilities_->tilt()));
    default_settings_->setZoom(RangeMean(all_capabilities_->zoom()));
    default_settings_->setTorch(false);
    default_settings_->setBackgroundBlur(true);
    default_settings_->setEyeGazeCorrection(false);
    default_settings_->setFaceFraming(false);
    default_settings_->setBackgroundSegmentationMask(false);
    // Capabilities and default settings must be chosen so that at least
    // the constraint set {exposureCompensation: {max: ...}} with
    // `all_capabilities_->exposureCompensation()->min() +
    //  kExposureCompensationDelta` is not satisfied by the default settings.
    // Otherwise `CheckMaxValues` does not really check anything.
    DCHECK_LT(all_capabilities_->exposureCompensation()->min() +
                  kExposureCompensationDelta,
              default_settings_->exposureCompensation());
    // Capabilities and default settings must be chosen so that at least
    // the constraint set {focusDistance: {min: ...}} with
    // `all_capabilities_->focusDistance()->min() +
    //  kFocusDistanceDelta` is not satisfied by the default settings.
    // Otherwise `CheckMinValues` does not really check anything.
    DCHECK_GT(all_capabilities_->focusDistance()->min() + kFocusDistanceDelta,
              default_settings_->focusDistance());
  }

 protected:
  void SetUp() override {
    image_capture_->SetCapabilitiesForTesting(all_capabilities_);
    image_capture_->SetSettingsForTesting(default_settings_);
  }

  void TearDown() override {
    image_capture_->SetExecutionContext(nullptr);
    ImageCaptureTest::TearDown();
  }

  Persistent<MediaTrackCapabilities> all_capabilities_;
  Persistent<MediaTrackCapabilities> all_non_capabilities_;
  Persistent<MediaTrackSettings> default_settings_;
};

TEST_F(ImageCaptureConstraintTest, ApplyBasicBareValueConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: "...",
  //     exposureMode: ["...", ...],
  //     focusMode: ["...", ...],
  //     exposureCompensation: ...,
  //     ...
  //   }
  auto* constraints = MediaTrackConstraints::Create();
  PopulateConstraintSet<ConstrainWithBareValueCreator>(constraints,
                                                       all_capabilities_);
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckExactValues(settings, all_capabilities_);

  // Create constraints: {exposureCompensation: ...}
  constraints = MediaTrackConstraints::Create();
  constraints->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          all_capabilities_->exposureCompensation()->max() + 1));
  settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the closest setting within the capability range and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  EXPECT_TRUE(settings->has_exposure_compensation);
  EXPECT_EQ(settings->exposure_compensation,
            all_capabilities_->exposureCompensation()->max());
}

TEST_F(ImageCaptureConstraintTest, ApplyBasicExactConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: {exact: "..."},
  //     exposureMode: {exact: ["...", ...]},
  //     focusMode: {exact: ["...", ...]},
  //     exposureCompensation: {exact: ...},
  //     ...
  //   }
  auto* constraints = MediaTrackConstraints::Create();
  PopulateConstraintSet<ConstrainWithExactDictionaryCreator>(constraints,
                                                             all_capabilities_);
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckExactValues(settings, all_capabilities_);
}

TEST_F(ImageCaptureConstraintTest, ApplyBasicIdealConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: {ideal: "..."},
  //     exposureMode: {ideal: ["...", ...]},
  //     focusMode: {ideal: ["...", ...]},
  //     exposureCompensation: {ideal: ...},
  //     ...
  //   }
  auto* full_constraints = MediaTrackConstraints::Create();
  PopulateConstraintSet<ConstrainWithIdealDictionaryCreator>(full_constraints,
                                                             all_capabilities_);
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, full_constraints, resolver));
  CheckExactValues(settings, all_capabilities_);

  // Create constraints: {exposureCompensation: {ideal: ...}}
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstrainWithIdealDictionaryCreator::Create(
              all_capabilities_->exposureCompensation()->max() + 1)));
  settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the closest setting within the capability range and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  EXPECT_TRUE(settings->has_exposure_compensation);
  EXPECT_EQ(settings->exposure_compensation,
            all_capabilities_->exposureCompensation()->max());

  // Reuse `full_constraints` but remove capabilities.
  image_capture_->SetCapabilitiesForTesting(
      MakeGarbageCollected<MediaTrackCapabilities>());
  settings = media::mojom::blink::PhotoSettings::New();
  // Shuold ignore ideal constraints without capabilities and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, full_constraints, resolver));
  CheckNoValues(settings, full_constraints->pointsOfInterest()
                              ->GetAsConstrainPoint2DParameters()
                              ->ideal()
                              .size());
}

TEST_F(ImageCaptureConstraintTest, ApplyBasicMaxConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: {},
  //     exposureMode: {},
  //     focusMode: {},
  //     exposureCompensation: {max: ...},
  //     ...
  //   }
  auto* constraints = MediaTrackConstraints::Create();
  PopulateConstraintSet<ConstrainWithMaxOrEmptyDictionaryCreator>(
      constraints, all_capabilities_);
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the max constraints to the current settings and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckMaxValues(settings, all_capabilities_, default_settings_);
}

TEST_F(ImageCaptureConstraintTest, ApplyBasicMinConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: {},
  //     exposureMode: {},
  //     focusMode: {},
  //     exposureCompensation: {min: ...},
  //     ...
  //   }
  auto* constraints = MediaTrackConstraints::Create();
  PopulateConstraintSet<ConstrainWithMinOrEmptyDictionaryCreator>(
      constraints, all_capabilities_);
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the min constraints to the current settings and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckMinValues(settings, all_capabilities_, default_settings_);
}

// If an empty list has been given as the value for a constraint, it MUST be
// interpreted as if the constraint were not specified (in other words,
// an empty constraint == no constraint).
// https://w3c.github.io/mediacapture-main/#dfn-selectsettings
TEST_F(ImageCaptureConstraintTest, ApplyBasicNoConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {
  //     whiteBalanceMode: [],
  //     exposureMode: {exact: []},
  //     focusMode: {ideal: []},
  //     pointsOfInterest: {exact: []}
  //   }
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          Vector<String>()));
  constraints->setExposureMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithExactDictionaryCreator::Create(Vector<String>())));
  constraints->setFocusMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithIdealDictionaryCreator::Create(Vector<String>())));
  constraints->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          ConstrainWithExactDictionaryCreator::Create(
              HeapVector<Member<Point2D>>())));
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should ignore empty sequences and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckNoValues(settings);
}

TEST_F(ImageCaptureConstraintTest, ApplyBasicOverconstrainedConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto settings = media::mojom::blink::PhotoSettings::New();

  // Create constraints: {whiteBalanceMode: {exact: "..."}}
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithExactDictionaryCreator::Create(
              all_non_capabilities_->whiteBalanceMode()[0])));
  auto* capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "whiteBalanceMode");

  // Create constraints: {whiteBalanceMode: {exact: ["..."]}}
  constraints = MediaTrackConstraints::Create();
  constraints->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithExactDictionaryCreator::Create(
              all_non_capabilities_->whiteBalanceMode())));
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "whiteBalanceMode");

  // Create constraints: {exposureCompensation: {exact: ...}}
  constraints = MediaTrackConstraints::Create();
  constraints->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstrainWithExactDictionaryCreator::Create(
              all_capabilities_->exposureCompensation()->min() - 1)));
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "exposureCompensation");

  // Create constraints: {exposureCompensation: {max: ...}}
  constraints = MediaTrackConstraints::Create();
  constraints->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstrainWithMaxDictionaryCreator::Create(
              all_capabilities_->exposureCompensation()->min() - 1)));
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "exposureCompensation");

  // Create constraints: {exposureCompensation: {min: ...}}
  constraints = MediaTrackConstraints::Create();
  constraints->setExposureCompensation(
      MakeGarbageCollected<V8UnionConstrainDoubleRangeOrDouble>(
          ConstrainWithMinDictionaryCreator::Create(
              all_capabilities_->exposureCompensation()->max() + 1)));
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "exposureCompensation");

  // Create constraints: {backgroundBlur: {exact: ...}}
  constraints = MediaTrackConstraints::Create();
  constraints->setBackgroundBlur(
      MakeGarbageCollected<V8UnionBooleanOrConstrainBooleanParameters>(
          ConstrainWithExactDictionaryCreator::Create(
              all_non_capabilities_->backgroundBlur()[0])));
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "backgroundBlur");

  // Reuse previous constraints but remove capabilities.
  image_capture_->SetCapabilitiesForTesting(
      MakeGarbageCollected<MediaTrackCapabilities>());
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Message(), "Unsupported constraint");
}

TEST_F(ImageCaptureConstraintTest, ApplyFirstAdvancedBareValueConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {
  //       whiteBalanceMode: "...",
  //       exposureMode: ["...", ...],
  //       focusMode: ["...", ...],
  //       exposureCompensation: ...,
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithBareValueCreator>(constraint_set,
                                                       all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  // TODO(crbug.com/1408091): This is not spec compliant.
  // ImageCapture should support DOMString sequence constraints (used above for
  // exposureMode and focusMode) in the first advanced constraint set.
  CheckExactValues(settings, all_capabilities_, ExpectHasPanTiltZoom(true),
                   ExpectHasExposureModeAndFocusMode(false));
}

TEST_F(ImageCaptureConstraintTest, ApplyFirstAdvancedExactConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {
  //       whiteBalanceMode: {exact: "..."},
  //       exposureMode: {exact: ["...", ...]},
  //       focusMode: {exact: ["...", ...]},
  //       exposureCompensation: {exact: ...},
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithExactDictionaryCreator>(constraint_set,
                                                             all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  // TODO(crbug.com/1408091): This is not spec compliant.
  // ImageCapture should support non-bare value constraints in the first
  // advanced constraint set.
  CheckNoValues(settings);
}

TEST_F(ImageCaptureConstraintTest, ApplyFirstAdvancedIdealConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {
  //       whiteBalanceMode: {ideal: "..."},
  //       exposureMode: {ideal: ["...", ...]},
  //       focusMode: {ideal: ["...", ...]},
  //       exposureCompensation: {ideal: ...},
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithIdealDictionaryCreator>(constraint_set,
                                                             all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Shuold ignore ideal constraints in advanced constraint sets and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  // The fitness distance
  // (https://w3c.github.io/mediacapture-main/#dfn-fitness-distance) between
  // an ideal constraint and a setting in a settings dictionary is always
  // between 0.0 and 1.0 (inclusive).
  // Therefore, the fitness distance between a constraint set containing only
  // ideal constraints and a settings dictionary (being the sum of the above
  // fitness distances in [0.0, 1.0]) is always finite.
  // On the other hand, the SelectSettings algorithm
  // (https://w3c.github.io/mediacapture-main/#dfn-selectsettings) iterates
  // over the advanced constraint sets and computes the fitness distance
  // between the advanced constraint sets and each settings dictionary
  // candidate and if the fitness distance is finite for one or more settings
  // dictionary candidates, it keeps those settings dictionary candidates.
  //
  // All in all, in this test case all the fitness distances are finite and
  // therefore the SelectSettings algorithm keeps all settings dictionary
  // candidates instead of favouring a particular settings dictionary and
  // therefore `CheckAndApplyMediaTrackConstraintsToSettings` does not set
  // settings in `settings`.
  CheckNoValues(settings);
}

TEST_F(ImageCaptureConstraintTest,
       ApplyFirstAdvancedOverconstrainedConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  const HeapVector<Member<Point2D>> points_of_interest = {
      CreatePoint2D(0.25, 0.75)};
  auto settings = media::mojom::blink::PhotoSettings::New();

  // Create constraints: {advanced: [{whiteBalanceMode: "..."}]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          all_non_capabilities_->whiteBalanceMode()[0]));
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  auto* capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  // TODO(crbug.com/1408091): This is not spec compliant. This should not fail.
  // Instead, should discard the first advanced constraint set and succeed.
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "whiteBalanceMode");

  // Create constraints: {advanced: [{pointsOfInterest: [...], pan: false}]}
  constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          points_of_interest));
  constraint_set->setPan(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          false));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  // TODO(crbug.com/1408091): This is not spec compliant. This should not fail.
  // Instead, should discard the first advanced constraint set and succeed.
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "pan");

  // Remove capabilities (does not affect pointsOfInterest).
  image_capture_->SetCapabilitiesForTesting(
      MakeGarbageCollected<MediaTrackCapabilities>());
  // Create constraints: {advanced: [{pointsOfInterest: [...], pan: true}]}
  constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          points_of_interest));
  constraint_set->setPan(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(true));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  // TODO(crbug.com/1408091): This is not spec compliant. This should not fail.
  // Instead, should discard the first advanced constraint set and succeed.
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "OverconstrainedError");
  EXPECT_EQ(capture_error->Constraint(), "pan");
}

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedBareValueConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {},
  //     {
  //       whiteBalanceMode: "...",
  //       exposureMode: ["...", ...],
  //       focusMode: ["...", ...],
  //       exposureCompensation: ...,
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithBareValueCreator>(constraint_set,
                                                       all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckExactValues(settings, all_capabilities_);
}

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedExactConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {},
  //     {
  //       whiteBalanceMode: {exact: "..."},
  //       exposureMode: {exact: ["...", ...]},
  //       focusMode: {exact: ["...", ...]},
  //       exposureCompensation: {exact: ...},
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithExactDictionaryCreator>(constraint_set,
                                                             all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should apply the constraints to the settings as is and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckExactValues(settings, all_capabilities_);
}

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedIdealConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {},
  //     {
  //       whiteBalanceMode: {ideal: "..."},
  //       exposureMode: {ideal: ["...", ...]},
  //       focusMode: {ideal: ["...", ...]},
  //       exposureCompensation: {ideal: ...},
  //       ...
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  PopulateConstraintSet<ConstrainWithIdealDictionaryCreator>(constraint_set,
                                                             all_capabilities_);
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Shuold ignore ideal constraints in advanced constraint sets and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  // The fitness distance
  // (https://w3c.github.io/mediacapture-main/#dfn-fitness-distance) between
  // an ideal constraint and a setting in a settings dictionary is always
  // between 0.0 and 1.0 (inclusive).
  // Therefore, the fitness distance between a constraint set containing only
  // ideal constraints and a settings dictionary (being the sum of the above
  // fitness distances in [0.0, 1.0]) is always finite.
  // On the other hand, the SelectSettings algorithm
  // (https://w3c.github.io/mediacapture-main/#dfn-selectsettings) iterates
  // over the advanced constraint sets and computes the fitness distance
  // between the advanced constraint sets and each settings dictionary
  // candidate and if the fitness distance is finite for one or more settings
  // dictionary candidates, it keeps those settings dictionary candidates.
  //
  // All in all, in this test case all the fitness distances are finite and
  // therefore the SelectSettings algorithm keeps all settings dictionary
  // candidates instead of favouring a particular settings dictionary and
  // therefore `CheckAndApplyMediaTrackConstraintsToSettings` does not set
  // settings in `settings`.
  CheckNoValues(settings);
}

// If an empty list has been given as the value for a constraint, it MUST be
// interpreted as if the constraint were not specified (in other words,
// an empty constraint == no constraint).
// https://w3c.github.io/mediacapture-main/#dfn-selectsettings
TEST_F(ImageCaptureConstraintTest, ApplyAdvancedNoConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints:
  //   {advanced: [
  //     {},
  //     {
  //       whiteBalanceMode: [],
  //       exposureMode: {exact: []},
  //       focusMode: {ideal: []},
  //       pointsOfInterest: {exact: []}
  //     }
  //   ]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          Vector<String>()));
  constraint_set->setExposureMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithExactDictionaryCreator::Create(Vector<String>())));
  constraint_set->setFocusMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          ConstrainWithIdealDictionaryCreator::Create(Vector<String>())));
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          ConstrainWithExactDictionaryCreator::Create(
              HeapVector<Member<Point2D>>())));
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should ignore empty sequences and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckNoValues(settings);
}

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedOverconstrainedConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  const HeapVector<Member<Point2D>> points_of_interest = {
      CreatePoint2D(0.25, 0.75)};
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  // Create constraints: {advanced: [{}, {whiteBalanceMode: "..."}]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          all_non_capabilities_->whiteBalanceMode()[0]));
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  auto settings = media::mojom::blink::PhotoSettings::New();
  // Should discard the last advanced constraint set and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckNoValues(settings);

  // Create constraints: {advanced: [{}, {pointsOfInterest: [...], pan: false}]}
  constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          points_of_interest));
  constraint_set->setPan(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          false));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  settings = media::mojom::blink::PhotoSettings::New();
  // Should discard the last advanced constraint set and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckNoValues(settings);

  // Remove capabilities (does not affect pointsOfInterest).
  image_capture_->SetCapabilitiesForTesting(
      MakeGarbageCollected<MediaTrackCapabilities>());
  // Create constraints: {advanced: [{}, {pointsOfInterest: [...], pan: true}]}
  constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setPointsOfInterest(
      MakeGarbageCollected<V8UnionConstrainPoint2DParametersOrPoint2DSequence>(
          points_of_interest));
  constraint_set->setPan(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(true));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  settings = media::mojom::blink::PhotoSettings::New();
  // Should discard the last advanced constraint set and succeed.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  CheckNoValues(settings);
}

// If the visibilityState of the top-level browsing context value is "hidden",
// the `applyConstraints()` algorithm MUST throw a `SecurityError` if `pan`,
// `tilt` or `zoom` dictionary member exists with a value other than `false`.
// https://w3c.github.io/mediacapture-image/#pan
// https://w3c.github.io/mediacapture-image/#tilt
// https://w3c.github.io/mediacapture-image/#zoom
TEST_F(ImageCaptureConstraintTest, ApplySecurityErrorConstraints) {
  V8TestingScope scope;
  scope.GetPage().SetVisibilityState(blink::mojom::PageVisibilityState::kHidden,
                                     /*is_initial_state=*/true);
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto settings = media::mojom::blink::PhotoSettings::New();

  // Create constraints: {pan: ...}
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setPan(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          default_settings_->pan()));
  auto* capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "SecurityError");

  // Create constraints: {advanced: [{tilt: ...}]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setTilt(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          default_settings_->tilt()));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "SecurityError");

  // Create constraints: {advanced: [{}, {zoom: ...}]}
  constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setZoom(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          default_settings_->zoom()));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  EXPECT_FALSE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
  scope.PerformMicrotaskCheckpoint();  // Resolve/reject promises.
  EXPECT_TRUE(capture_error->WasCalled());
  EXPECT_EQ(capture_error->Name(), "SecurityError");
}

TEST_F(ImageCaptureTest, GrabFrameOfLiveTrackIsFulfilled) {
  V8TestingScope scope;
  SetupTrackMocks(scope);
  track_->SetReadyState("live");
  track_->setEnabled(true);
  track_->SetMuted(false);

  ScriptPromiseUntyped result =
      image_capture_->grabFrame(scope.GetScriptState());

  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(ImageCaptureTest, GrabFrameOfMutedTrackIsFulfilled) {
  V8TestingScope scope;
  SetupTrackMocks(scope);
  track_->SetReadyState("live");
  track_->setEnabled(true);
  track_->SetMuted(true);

  ScriptPromiseUntyped result =
      image_capture_->grabFrame(scope.GetScriptState());

  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(ImageCaptureTest, GrabFrameOfMutedTrackWithoutFramesIsRejected) {
  V8TestingScope scope;
  SetupTrackMocks(scope, /*produce_frame_on_add_sink=*/false);
  track_->SetReadyState("live");
  track_->setEnabled(true);
  track_->SetMuted(true);

  ScriptPromiseUntyped result =
      image_capture_->grabFrame(scope.GetScriptState());

  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(ImageCaptureTest, GrabFrameOfEndedTrackRejects) {
  V8TestingScope scope;
  track_->SetReadyState("ended");
  track_->setEnabled(true);
  track_->SetMuted(false);

  ScriptPromiseUntyped result =
      image_capture_->grabFrame(scope.GetScriptState());

  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(ImageCaptureTest, GrabFrameOfDisabledTrackRejects) {
  V8TestingScope scope;
  track_->SetReadyState("live");
  track_->setEnabled(false);
  track_->SetMuted(false);

  ScriptPromiseUntyped result =
      image_capture_->grabFrame(scope.GetScriptState());

  ScriptPromiseTester tester(scope.GetScriptState(), result);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

}  // namespace blink
