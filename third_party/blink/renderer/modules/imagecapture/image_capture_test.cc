// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

using ExpectHasPanTiltZoom =
    base::StrongAlias<class ExpectHasPanTiltZoomTag, bool>;
using ExpectHasExposureModeAndFocusMode =
    base::StrongAlias<class ExpectHasExposureModeAndFocusModeTag, bool>;
using PopulatePanTiltZoom =
    base::StrongAlias<class PopulatePanTiltZoomZoomTag, bool>;

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
    name_ = ToCoreString(name->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();
    message_ = ToCoreString(message->ToString(context).ToLocalChecked());
    v8::Local<v8::Value> constraint =
        error_object->Get(context, V8String(isolate, "constraint"))
            .ToLocalChecked();
    constraint_ = ToCoreString(constraint->ToString(context).ToLocalChecked());

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
  EXPECT_EQ(settings->points_of_interest[0]->x, -0.75);
  EXPECT_EQ(settings->points_of_interest[0]->y, -0.25);
  EXPECT_EQ(settings->points_of_interest[1]->x, 0.25);
  EXPECT_EQ(settings->points_of_interest[1]->y, 0.75);
  EXPECT_EQ(settings->points_of_interest[2]->x, 1.25);
  EXPECT_EQ(settings->points_of_interest[2]->y, 1.75);
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
}

void CheckNoValues(const media::mojom::blink::PhotoSettingsPtr& settings) {
  EXPECT_FALSE(settings->has_white_balance_mode);
  EXPECT_FALSE(settings->has_exposure_mode);
  EXPECT_FALSE(settings->has_focus_mode);
  EXPECT_EQ(settings->points_of_interest.size(), 0u);
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
}

}  // namespace

class ImageCaptureTest : public testing::Test {
 public:
  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

 protected:
  ImageCapture* CreateImageCapture(bool pan_tilt_zoom_allowed = true) const {
    constexpr ExecutionContext* execution_context = nullptr;
    MediaStreamTrack* track = MakeGarbageCollected<MockMediaStreamTrack>();
    return MakeGarbageCollected<ImageCapture>(
        execution_context, track, pan_tilt_zoom_allowed, base::DoNothing());
  }
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
    all_non_capabilities_->setBackgroundBlur({false});
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
    default_settings_->setBackgroundBlur(false);
    // Capabilities and default settings must be chosen so that at least
    // the constraint set {exposureCompensation: {max: ...}} with
    // `all_capabilities_->exposureCompensation()->min() +
    //  kExposureCompensationDelta` is not satisfied by the default settings.
    DCHECK_LT(all_capabilities_->exposureCompensation()->min() +
                  kExposureCompensationDelta,
              default_settings_->exposureCompensation());
    // Capabilities and default settings must be chosen so that at least
    // the constraint set {focusDistance: {min: ...}} with
    // `all_capabilities_->focusDistance()->min() +
    //  kFocusDistanceDelta` is not satisfied by the default settings.
    DCHECK_GT(all_capabilities_->focusDistance()->min() + kFocusDistanceDelta,
              default_settings_->focusDistance());
    image_capture_ = CreateImageCapture();
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
  Persistent<ImageCapture> image_capture_;
};

TEST_F(ImageCaptureConstraintTest, ApplyFirstAdvancedBareValueConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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
}

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedIdealConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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

TEST_F(ImageCaptureConstraintTest, ApplyAdvancedOverconstrainedConstraints) {
  V8TestingScope scope;
  image_capture_->SetExecutionContext(scope.GetExecutionContext());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  auto settings = media::mojom::blink::PhotoSettings::New();

  // Create constraints: {advanced: [{}, {whiteBalanceMode: "..."}]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setWhiteBalanceMode(
      MakeGarbageCollected<
          V8UnionConstrainDOMStringParametersOrStringOrStringSequence>(
          all_non_capabilities_->whiteBalanceMode()[0]));
  auto* constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({MediaTrackConstraintSet::Create(), constraint_set});
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
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  // TODO(crbug.com/1408091): This is not spec compliant. This should fail with
  // a `SecurityError`.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));

  // Create constraints: {advanced: [{tilt: ...}]}
  auto* constraint_set = MediaTrackConstraintSet::Create();
  constraint_set->setTilt(
      MakeGarbageCollected<V8UnionBooleanOrConstrainDoubleRangeOrDouble>(
          default_settings_->tilt()));
  constraints = MediaTrackConstraints::Create();
  constraints->setAdvanced({constraint_set});
  capture_error = MakeGarbageCollected<CaptureErrorFunction>();
  resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
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
  resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  resolver->Promise().Then(nullptr, MakeGarbageCollected<ScriptFunction>(
                                        scope.GetScriptState(), capture_error));
  // TODO(crbug.com/1408091): This is not spec compliant. This should fail with
  // a `SecurityError`.
  EXPECT_TRUE(image_capture_->CheckAndApplyMediaTrackConstraintsToSettings(
      &*settings, constraints, resolver));
}

}  // namespace blink
