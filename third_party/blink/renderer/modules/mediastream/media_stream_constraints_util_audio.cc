// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/webrtc/webrtc_switches.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"

// TODO(crbug.com/704136): Replace the use of std::vector by WTF::Vector.

namespace blink {

using blink::AudioCaptureSettings;
using blink::AudioProcessingProperties;
using ConstraintSet = blink::WebMediaTrackConstraintSet;
using BooleanConstraint = blink::BooleanConstraint;
using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;
using ProcessingType = AudioCaptureSettings::ProcessingType;
using StringConstraint = blink::StringConstraint;

template <class T>
using NumericRangeSet = blink::media_constraints::NumericRangeSet<T>;

namespace {

using BoolSet = blink::media_constraints::DiscreteSet<bool>;
using DoubleRangeSet = blink::media_constraints::NumericRangeSet<double>;
using EchoCancellationTypeSet =
    blink::media_constraints::DiscreteSet<EchoCancellationType>;
using IntRangeSet = blink::media_constraints::NumericRangeSet<int>;
using StringSet = blink::media_constraints::DiscreteSet<std::string>;

// The presence of a MediaStreamAudioSource object indicates whether the source
// in question is currently in use, or not. This convenience enum helps
// identifying whether a source is available and, if so, whether it has audio
// processing enabled or disabled.
enum class SourceType { kNone, kUnprocessed, kNoApmProcessed, kApmProcessed };

// The sample size is set to 16 due to the Signed-16 format representation.
int32_t GetSampleSize() {
  return media::SampleFormatToBitsPerChannel(media::kSampleFormatS16);
}

// This class encapsulates two values that together build up the score of each
// processed candidate.
// - Fitness, similarly defined by the W3C specification
//   (https://w3c.github.io/mediacapture-main/#dfn-fitness-distance);
// - Distance from the default device ID;
// - The priority associated to the echo cancellation type selected.
// - The priority of the associated processing-based container.
//
// Differently from the definition in the W3C specification, the present
// algorithm maximizes the score.
struct Score {
 public:
  enum class EcModeScore : int {
    kDisabled = 1,
    kSystem = 2,
    kAec3 = 3,
  };

  explicit Score(double fitness,
                 bool is_default_device_id = false,
                 EcModeScore ec_mode_score = EcModeScore::kDisabled,
                 int processing_priority = -1) {
    score = std::make_tuple(fitness, is_default_device_id, ec_mode_score,
                            processing_priority);
  }

  bool operator>(const Score& other) const { return score > other.score; }

  Score& operator+=(const Score& other) {
    std::get<0>(score) += std::get<0>(other.score);
    std::get<1>(score) |= std::get<1>(other.score);
    // Among the priorities in the two score objects, we store the highest one.
    std::get<2>(score) = std::max(std::get<2>(score), std::get<2>(other.score));
    // Select the highest processing priority.
    std::get<3>(score) = std::max(std::get<3>(score), std::get<3>(other.score));
    return *this;
  }

  Score& operator+=(double fitness) {
    std::get<0>(score) += fitness;
    return *this;
  }

  Score& operator+=(bool is_default_device) {
    std::get<1>(score) |= is_default_device;
    return *this;
  }

  void set_ec_mode_score(EcModeScore ec_mode_score) {
    std::get<2>(score) = ec_mode_score;
  }

  void set_processing_priority(int priority) { std::get<3>(score) = priority; }

  std::tuple<double, bool, EcModeScore, int> score;
};

// This class represents the output of DeviceContainer::InfoFromSource and is
// used to obtain information regarding an active source, if that exists.
class SourceInfo {
 public:
  SourceInfo(SourceType type,
             const AudioProcessingProperties& properties,
             base::Optional<int> channels,
             base::Optional<int> sample_rate,
             base::Optional<double> latency)
      : type_(type),
        properties_(properties),
        channels_(std::move(channels)),
        sample_rate_(std::move(sample_rate)),
        latency_(latency) {}

  bool HasActiveSource() { return type_ != SourceType::kNone; }

  SourceType type() { return type_; }
  const AudioProcessingProperties& properties() { return properties_; }
  const base::Optional<int>& channels() { return channels_; }
  const base::Optional<int>& sample_rate() { return sample_rate_; }
  const base::Optional<double>& latency() { return latency_; }

 private:
  const SourceType type_;
  const AudioProcessingProperties properties_;
  const base::Optional<int> channels_;
  const base::Optional<int> sample_rate_;
  const base::Optional<double> latency_;
};

// Container for each independent boolean constrainable property.
class BooleanContainer {
 public:
  BooleanContainer(BoolSet allowed_values = BoolSet())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const BooleanConstraint& constraint) {
    allowed_values_ = allowed_values_.Intersection(
        blink::media_constraints::BoolSetFromConstraint(constraint));
    return allowed_values_.IsEmpty() ? constraint.GetName() : nullptr;
  }

  std::tuple<double, bool> SelectSettingsAndScore(
      const BooleanConstraint& constraint,
      bool default_setting) const {
    DCHECK(!IsEmpty());

    if (constraint.HasIdeal() && allowed_values_.Contains(constraint.Ideal()))
      return std::make_tuple(1.0, constraint.Ideal());

    if (allowed_values_.is_universal())
      return std::make_tuple(0.0, default_setting);

    DCHECK_EQ(allowed_values_.elements().size(), 1U);
    return std::make_tuple(0.0, allowed_values_.FirstElement());
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  BoolSet allowed_values_;
};

// Container for each independent string constrainable property.
class StringContainer {
 public:
  explicit StringContainer(StringSet allowed_values = StringSet())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const StringConstraint& constraint) {
    allowed_values_ = allowed_values_.Intersection(
        blink::media_constraints::StringSetFromConstraint(constraint));
    return allowed_values_.IsEmpty() ? constraint.GetName() : nullptr;
  }

  // Selects the best value from the nonempty |allowed_values_|, subject to
  // |constraint_set.*constraint_member_| and determines the associated fitness.
  // The first selection criteria is inclusion in the constraint's ideal value,
  // followed by equality to |default_value|. There is always a single best
  // value.
  std::tuple<double, std::string> SelectSettingsAndScore(
      const StringConstraint& constraint,
      std::string default_setting) const {
    DCHECK(!IsEmpty());
    if (constraint.HasIdeal()) {
      for (const blink::WebString& ideal_candidate : constraint.Ideal()) {
        std::string candidate = ideal_candidate.Utf8();
        if (allowed_values_.Contains(candidate))
          return std::make_tuple(1.0, candidate);
      }
    }

    std::string setting = allowed_values_.Contains(default_setting)
                              ? default_setting
                              : allowed_values_.FirstElement();

    return std::make_tuple(0.0, setting);
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  StringSet allowed_values_;
};

// Container for each independent numeric constrainable property.
template <class T, class C>
class NumericContainer {
 public:
  explicit NumericContainer(
      NumericRangeSet<T> allowed_values = NumericRangeSet<T>())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const C& constraint) {
    auto constraint_set = NumericRangeSet<T>::FromConstraint(constraint);
    allowed_values_ = allowed_values_.Intersection(constraint_set);
    return IsEmpty() ? constraint.GetName() : nullptr;
  }

  // This function will return a fitness with the associated setting for channel
  // count. The setting will be the ideal value, if such value is provided and
  // admitted, or the closest value to it.
  // When no ideal is available and |default_setting| is provided, the setting
  // will be |default_setting| or the closest value to it.
  // When |default_setting| is **not** provided, the setting will be a value iff
  // |allowed_values_| contains only a single value, otherwise base::nullopt is
  // returned to signal that it was not possible to make a decision.
  std::tuple<double, base::Optional<T>> SelectSettingsAndScore(
      const C& constraint,
      const base::Optional<T>& default_setting = base::nullopt) const {
    DCHECK(!IsEmpty());

    if (constraint.HasIdeal()) {
      if (allowed_values_.Contains(constraint.Ideal()))
        return std::make_tuple(1.0, constraint.Ideal());

      T value = SelectClosestValueTo(constraint.Ideal());
      double fitness = 1.0 - blink::NumericConstraintFitnessDistance(
                                 value, constraint.Ideal());
      return std::make_tuple(fitness, value);
    }

    if (default_setting) {
      if (allowed_values_.Contains(*default_setting))
        return std::make_tuple(0.0, *default_setting);

      // If the default value provided is not contained, select the value
      // closest to it.
      return std::make_tuple(0.0, SelectClosestValueTo(*default_setting));
    }

    if (allowed_values_.Min() && allowed_values_.Max() &&
        *allowed_values_.Min() == *allowed_values_.Max()) {
      return std::make_tuple(0.0, *allowed_values_.Min());
    }

    return std::make_tuple(0.0, base::nullopt);
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  T SelectClosestValueTo(T value) const {
    DCHECK(allowed_values_.Min() || allowed_values_.Max());
    DCHECK(!allowed_values_.Contains(value));
    return allowed_values_.Min() && value < *allowed_values_.Min()
               ? *allowed_values_.Min()
               : *allowed_values_.Max();
  }

  NumericRangeSet<T> allowed_values_;
};

using IntegerContainer = NumericContainer<int, blink::LongConstraint>;
using DoubleContainer = NumericContainer<double, blink::DoubleConstraint>;

// Container to manage the properties related to echo cancellation:
// echoCancellation, googEchoCancellation and echoCancellationType.
class EchoCancellationContainer {
 public:
  // Default constructor intended to temporarily create an empty object.
  EchoCancellationContainer()
      : ec_mode_allowed_values_(EchoCancellationTypeSet::EmptySet()),
        device_parameters_(media::AudioParameters::UnavailableDeviceParams()),
        is_device_capture_(true) {}

  EchoCancellationContainer(std::vector<EchoCancellationType> allowed_values,
                            bool has_active_source,
                            bool is_device_capture,
                            media::AudioParameters device_parameters,
                            AudioProcessingProperties properties,
                            bool is_reconfiguration_allowed)
      : ec_mode_allowed_values_(EchoCancellationTypeSet({allowed_values})),
        device_parameters_(device_parameters),
        is_device_capture_(is_device_capture) {
    if (!has_active_source) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
      // If force system echo cancellation feature is enabled, only expose that
      // type if available; otherwise expose no type.
      if (base::FeatureList::IsEnabled(features::kForceEnableSystemAec)) {
        ec_mode_allowed_values_ =
            ec_mode_allowed_values_.Intersection(EchoCancellationTypeSet(
                {EchoCancellationType::kEchoCancellationSystem,
                 EchoCancellationType::kEchoCancellationDisabled}));
      }
#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)
      return;
    }

    // If HW echo cancellation is used, reconfiguration is not supported and
    // only the current values are allowed. Otherwise, allow all possible values
    // for echo cancellation.
    if (is_reconfiguration_allowed &&
        properties.echo_cancellation_type !=
            EchoCancellationType::kEchoCancellationSystem) {
      return;
    }

    ec_mode_allowed_values_ =
        EchoCancellationTypeSet({properties.echo_cancellation_type});
    ec_allowed_values_ =
        BoolSet({properties.echo_cancellation_type !=
                 EchoCancellationType::kEchoCancellationDisabled});
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    // Convert the constraints into discrete sets.
    BoolSet ec_set = blink::media_constraints::BoolSetFromConstraint(
        constraint_set.echo_cancellation);
    BoolSet goog_ec_set = blink::media_constraints::BoolSetFromConstraint(
        constraint_set.goog_echo_cancellation);

    // Apply echoCancellation constraint.
    ec_allowed_values_ = ec_allowed_values_.Intersection(ec_set);
    if (ec_allowed_values_.IsEmpty())
      return constraint_set.echo_cancellation.GetName();
    // Intersect echoCancellation with googEchoCancellation and determine if
    // there is a contradiction.
    auto ec_intersection = ec_allowed_values_.Intersection(goog_ec_set);
    if (ec_intersection.IsEmpty())
      return constraint_set.echo_cancellation.GetName();
    // Translate the boolean values into EC modes.
    ec_mode_allowed_values_ = ec_mode_allowed_values_.Intersection(
        ToEchoCancellationTypes(ec_intersection));

    // Finally, if this container is empty, fail due to contradiction of the
    // resulting allowed values for goog_ec, ec, and/or ec_type.
    return IsEmpty() ? constraint_set.echo_cancellation.GetName() : nullptr;
  }

  std::tuple<Score, EchoCancellationType> SelectSettingsAndScore(
      const ConstraintSet& constraint_set) const {
    EchoCancellationType selected_ec_mode = SelectBestEcMode(constraint_set);
    double fitness =
        Fitness(selected_ec_mode, constraint_set.echo_cancellation);
    Score score(fitness);
    score.set_ec_mode_score(GetEcModeScore(selected_ec_mode));
    return std::make_tuple(score, selected_ec_mode);
  }

  bool IsEmpty() const { return ec_mode_allowed_values_.IsEmpty(); }

  // Audio-processing properties are disabled by default for content capture,
  // or if the |echo_cancellation| constraint is false.
  void UpdateDefaultValues(
      const BooleanConstraint& echo_cancellation_constraint,
      AudioProcessingProperties* properties) const {
    bool default_audio_processing_value =
        GetDefaultValueForAudioProperties(echo_cancellation_constraint);

    properties->goog_auto_gain_control &= default_audio_processing_value;
    properties->goog_experimental_echo_cancellation &=
        default_audio_processing_value;
    properties->goog_noise_suppression &= default_audio_processing_value;
    properties->goog_experimental_noise_suppression &=
        default_audio_processing_value;
    properties->goog_highpass_filter &= default_audio_processing_value;
    properties->goog_experimental_auto_gain_control &=
        default_audio_processing_value;
  }

  bool GetDefaultValueForAudioProperties(
      const BooleanConstraint& ec_constraint) const {
    DCHECK(!ec_mode_allowed_values_.is_universal());

    if (ec_constraint.HasIdeal() &&
        ec_allowed_values_.Contains(ec_constraint.Ideal()))
      return is_device_capture_ && ec_constraint.Ideal();

    if (ec_allowed_values_.Contains(true))
      return is_device_capture_;

    return false;
  }

 private:
  static Score::EcModeScore GetEcModeScore(EchoCancellationType mode) {
    switch (mode) {
      case EchoCancellationType::kEchoCancellationDisabled:
        return Score::EcModeScore::kDisabled;
      case EchoCancellationType::kEchoCancellationSystem:
        return Score::EcModeScore::kSystem;
      case EchoCancellationType::kEchoCancellationAec3:
        return Score::EcModeScore::kAec3;
    }
  }

  static EchoCancellationTypeSet ToEchoCancellationTypes(const BoolSet ec_set) {
    std::vector<EchoCancellationType> types;

    if (ec_set.Contains(false))
      types.push_back(EchoCancellationType::kEchoCancellationDisabled);

    if (ec_set.Contains(true)) {
      types.push_back(EchoCancellationType::kEchoCancellationAec3);
      types.push_back(EchoCancellationType::kEchoCancellationSystem);
    }

    return EchoCancellationTypeSet(types);
  }

  static bool ShouldUseExperimentalSystemEchoCanceller(
      const media::AudioParameters& parameters) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    if (base::FeatureList::IsEnabled(features::kForceEnableSystemAec) &&
        (parameters.effects() &
         media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER)) {
      return true;
    }
#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)
    return false;
  }

  EchoCancellationType SelectBestEcMode(
      const ConstraintSet& constraint_set) const {
    DCHECK(!IsEmpty());
    DCHECK(!ec_mode_allowed_values_.is_universal());

    // Try to use an ideal candidate, if supplied.
    bool is_ec_preferred =
        ShouldUseEchoCancellation(constraint_set.echo_cancellation,
                                  constraint_set.goog_echo_cancellation);

    if (!is_ec_preferred &&
        ec_mode_allowed_values_.Contains(
            EchoCancellationType::kEchoCancellationDisabled)) {
      return EchoCancellationType::kEchoCancellationDisabled;
    }

    // If no ideal could be selected and the set contains only one value, pick
    // that one.
    if (ec_mode_allowed_values_.elements().size() == 1)
      return ec_mode_allowed_values_.FirstElement();

    // If no type has been selected, choose system if the device has the
    // ECHO_CANCELLER flag set. Never automatically enable an experimental
    // system echo canceller.
    if (device_parameters_.IsValid() &&
        ec_mode_allowed_values_.Contains(
            EchoCancellationType::kEchoCancellationSystem) &&
        (device_parameters_.effects() &
             media::AudioParameters::ECHO_CANCELLER ||
         ShouldUseExperimentalSystemEchoCanceller(device_parameters_))) {
      return EchoCancellationType::kEchoCancellationSystem;
    }

    // At this point we have at least two elements, hence the only two options
    // from which to select are either AEC3 or System, where AEC3 has higher
    // priority.
    if (ec_mode_allowed_values_.Contains(
            EchoCancellationType::kEchoCancellationAec3)) {
      return EchoCancellationType::kEchoCancellationAec3;
    }

    DCHECK(ec_mode_allowed_values_.Contains(
        EchoCancellationType::kEchoCancellationDisabled));
    return EchoCancellationType::kEchoCancellationDisabled;
  }

  // This function computes the fitness score of the given |ec_mode|. The
  // fitness is determined by the ideal values of |ec_constraint|. If |ec_mode|
  // satisfies the constraint, the fitness score results in a value of 1, and 0
  // otherwise. If no ideal value is specified, the fitness is 1.
  double Fitness(const EchoCancellationType& ec_mode,
                 const BooleanConstraint& ec_constraint) const {
    return ec_constraint.HasIdeal()
               ? ((ec_constraint.Ideal() &&
                   ec_mode !=
                       EchoCancellationType::kEchoCancellationDisabled) ||
                  (!ec_constraint.Ideal() &&
                   ec_mode == EchoCancellationType::kEchoCancellationDisabled))
               : 1.0;
  }

  bool EchoCancellationModeContains(bool ec) const {
    DCHECK(!ec_mode_allowed_values_.is_universal());

    if (ec) {
      return ec_mode_allowed_values_.Contains(
                 EchoCancellationType::kEchoCancellationAec3) ||
             ec_mode_allowed_values_.Contains(
                 EchoCancellationType::kEchoCancellationSystem);
    }

    return ec_mode_allowed_values_.Contains(
        EchoCancellationType::kEchoCancellationDisabled);
  }

  bool ShouldUseEchoCancellation(
      const BooleanConstraint& ec_constraint,
      const BooleanConstraint& goog_ec_constraint) const {
    DCHECK(!ec_mode_allowed_values_.is_universal());

    if (ec_constraint.HasIdeal() &&
        EchoCancellationModeContains(ec_constraint.Ideal()))
      return ec_constraint.Ideal();

    if (goog_ec_constraint.HasIdeal() &&
        EchoCancellationModeContains(goog_ec_constraint.Ideal()))
      return goog_ec_constraint.Ideal();

    // Echo cancellation is enabled by default for device capture and disabled
    // by default for content capture.
    if (EchoCancellationModeContains(true) &&
        EchoCancellationModeContains(false))
      return is_device_capture_;

    return EchoCancellationModeContains(true);
  }

  BoolSet ec_allowed_values_;
  EchoCancellationTypeSet ec_mode_allowed_values_;
  media::AudioParameters device_parameters_;
  bool is_device_capture_;
};

// This container represents the supported audio settings for a given type of
// audio source. In practice, there are three types of sources: processed using
// APM, processed without APM, and unprocessed.
class ProcessingBasedContainer {
 public:
  static ProcessingBasedContainer CreateRemoteApmProcessedContainer(
      const SourceInfo& source_info,
      bool is_device_capture,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kApmProcessed,
        {EchoCancellationType::kEchoCancellationAec3,
         EchoCancellationType::kEchoCancellationDisabled},
        BoolSet(), /* goog_audio_mirroring_set */
        BoolSet(), /* goog_auto_gain_control_set */
        BoolSet(), /* goog_experimental_echo_cancellation_set */
        BoolSet(), /* goog_noise_suppression_set */
        BoolSet(), /* goog_experimental_noise_suppression_set */
        BoolSet(), /* goog_highpass_filter_set */
        BoolSet(), /* goog_experimental_auto_gain_control_set */
        IntRangeSet::FromValue(GetSampleSize()), /* sample_size_range */
        IntRangeSet::FromValue(
            device_parameters.channels()), /* channels_range */
        IntRangeSet::FromValue(
            device_parameters.sample_rate()), /* sample_rate_range */
        source_info, is_device_capture, device_parameters,
        is_reconfiguration_allowed);
  }

  // Creates an instance of ProcessingBasedContainer for the WebRTC processed
  // source type. The source type allows (a) any type of echo cancellation,
  // though the system echo cancellation type depends on the availability of the
  // related |parameters.effects()|, and (b) any combination of processing
  // properties settings.
  static ProcessingBasedContainer CreateApmProcessedContainer(
      const SourceInfo& source_info,
      bool is_device_capture,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kApmProcessed,
        {EchoCancellationType::kEchoCancellationAec3,
         EchoCancellationType::kEchoCancellationDisabled},
        BoolSet(), /* goog_audio_mirroring_set */
        BoolSet(), /* goog_auto_gain_control_set */
        BoolSet(), /* goog_experimental_echo_cancellation_set */
        BoolSet(), /* goog_noise_suppression_set */
        BoolSet(), /* goog_experimental_noise_suppression_set */
        BoolSet(), /* goog_highpass_filter_set */
        BoolSet(), /* goog_experimental_auto_gain_control_set */
        IntRangeSet::FromValue(GetSampleSize()), /* sample_size_range */
        IntRangeSet::FromValue(1),               /* channels_range */
        IntRangeSet::FromValue(
            blink::kAudioProcessingSampleRate), /* sample_rate_range */
        source_info, is_device_capture, device_parameters,
        is_reconfiguration_allowed);
  }

  // Creates an instance of ProcessingBasedContainer for the processed source
  // type. The source type allows (a) either system echo cancellation, if
  // allowed by the |parameters.effects()|, or none, (b) enabled or disabled
  // audio mirroring, while (c) all other processing properties settings cannot
  // be enabled.
  static ProcessingBasedContainer CreateNoApmProcessedContainer(
      const SourceInfo& source_info,
      bool is_device_capture,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kNoApmProcessed,
        {EchoCancellationType::kEchoCancellationDisabled},
        BoolSet(),        /* goog_audio_mirroring_set */
        BoolSet({false}), /* goog_auto_gain_control_set */
        BoolSet({false}), /* goog_experimental_echo_cancellation_set */
        BoolSet({false}), /* goog_noise_suppression_set */
        BoolSet({false}), /* goog_experimental_noise_suppression_set */
        BoolSet({false}), /* goog_highpass_filter_set */
        BoolSet({false}), /* goog_experimental_auto_gain_control_set */
        IntRangeSet::FromValue(GetSampleSize()), /* sample_size_range */
        IntRangeSet::FromValue(
            device_parameters.channels()), /* channels_range */
        IntRangeSet::FromValue(
            device_parameters.sample_rate()), /* sample_rate_range */
        source_info, is_device_capture, device_parameters,
        is_reconfiguration_allowed);
  }

  // Creates an instance of ProcessingBasedContainer for the unprocessed source
  // type. The source type allows (a) either system echo cancellation, if
  // allowed by the |parameters.effects()|, or none, while (c) all processing
  // properties settings cannot be enabled.
  static ProcessingBasedContainer CreateUnprocessedContainer(
      const SourceInfo& source_info,
      bool is_device_capture,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kUnprocessed,
        {EchoCancellationType::kEchoCancellationDisabled},
        BoolSet({false}), /* goog_audio_mirroring_set */
        BoolSet({false}), /* goog_auto_gain_control_set */
        BoolSet({false}), /* goog_experimental_echo_cancellation_set */
        BoolSet({false}), /* goog_noise_suppression_set */
        BoolSet({false}), /* goog_experimental_noise_suppression_set */
        BoolSet({false}), /* goog_highpass_filter_set */
        BoolSet({false}), /* goog_experimental_auto_gain_control_set */
        IntRangeSet::FromValue(GetSampleSize()), /* sample_size_range */
        IntRangeSet::FromValue(
            device_parameters.channels()), /* channels_range */
        IntRangeSet::FromValue(
            device_parameters.sample_rate()), /* sample_rate_range */
        source_info, is_device_capture, device_parameters,
        is_reconfiguration_allowed);
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* failed_constraint_name = nullptr;

    failed_constraint_name =
        echo_cancellation_container_.ApplyConstraintSet(constraint_set);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        sample_size_container_.ApplyConstraintSet(constraint_set.sample_size);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        channels_container_.ApplyConstraintSet(constraint_set.channel_count);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        sample_rate_container_.ApplyConstraintSet(constraint_set.sample_rate);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        latency_container_.ApplyConstraintSet(constraint_set.latency);
    if (failed_constraint_name)
      return failed_constraint_name;

    for (auto& info : kBooleanPropertyContainerInfoMap) {
      failed_constraint_name =
          boolean_containers_[info.index].ApplyConstraintSet(
              constraint_set.*(info.constraint_member));
      if (failed_constraint_name)
        return failed_constraint_name;
    }
    return failed_constraint_name;
  }

  std::tuple<Score, AudioProcessingProperties, base::Optional<int>>
  SelectSettingsAndScore(const ConstraintSet& constraint_set,
                         bool should_disable_hardware_noise_suppression,
                         const media::AudioParameters& parameters) const {
    DCHECK(!IsEmpty());

    Score score(0.0);
    double sub_score(0.0);

    std::tie(sub_score, std::ignore) =
        sample_size_container_.SelectSettingsAndScore(
            constraint_set.sample_size, GetSampleSize());
    score += sub_score;

    base::Optional<int> channels;
    std::tie(sub_score, channels) = channels_container_.SelectSettingsAndScore(
        constraint_set.channel_count);
    DCHECK(channels);
    score += sub_score;

    base::Optional<int> sample_size;
    std::tie(sub_score, sample_size) =
        sample_rate_container_.SelectSettingsAndScore(
            constraint_set.sample_rate);
    DCHECK(sample_size != base::nullopt);
    score += sub_score;

    base::Optional<double> latency;
    std::tie(sub_score, latency) =
        latency_container_.SelectSettingsAndScore(constraint_set.latency);
    score += sub_score;

    // Only request an explicit change to the buffer size for the unprocessed
    // container, and only if it's based on a specific user constraint.
    base::Optional<int> requested_buffer_size;
    if (processing_type_ == ProcessingType::kUnprocessed && latency &&
        !constraint_set.latency.IsEmpty()) {
      int min_buffer_size, max_buffer_size;
      std::tie(min_buffer_size, max_buffer_size) =
          GetMinMaxBufferSizesForAudioParameters(parameters);
      requested_buffer_size = media::AudioLatency::GetExactBufferSize(
          base::TimeDelta::FromSecondsD(*latency), parameters.sample_rate(),
          parameters.frames_per_buffer(), min_buffer_size, max_buffer_size,
          max_buffer_size);
    }

    AudioProcessingProperties properties;
    Score ec_score(0.0);
    std::tie(ec_score, properties.echo_cancellation_type) =
        echo_cancellation_container_.SelectSettingsAndScore(constraint_set);
    score += ec_score;

    // Update the default settings for each audio-processing properties
    // according to |echo_cancellation| and whether the source considered is
    // device capture.
    echo_cancellation_container_.UpdateDefaultValues(
        constraint_set.echo_cancellation, &properties);

    for (size_t i = 0; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      std::tie(sub_score, properties.*(info.property_member)) =
          boolean_containers_[info.index].SelectSettingsAndScore(
              constraint_set.*(info.constraint_member),
              properties.*(info.property_member));
      score += sub_score;
    }

    score.set_processing_priority(
        GetProcessingPriority(constraint_set.echo_cancellation));
    return std::make_tuple(score, properties, requested_buffer_size);
  }

  // The ProcessingBasedContainer is considered empty if at least one of the
  // containers owned by it is empty.
  bool IsEmpty() const {
    DCHECK(!boolean_containers_.empty());

    for (auto& container : boolean_containers_) {
      if (container.IsEmpty())
        return true;
    }
    return echo_cancellation_container_.IsEmpty() ||
           sample_size_container_.IsEmpty() || channels_container_.IsEmpty() ||
           sample_rate_container_.IsEmpty() || latency_container_.IsEmpty();
  }

  ProcessingType processing_type() const { return processing_type_; }

 private:
  enum BooleanContainerId {
    kGoogAudioMirroring,
    kGoogAutoGainControl,
    kGoogExperimentalEchoCancellation,
    kGoogNoiseSuppression,
    kGoogExperimentalNoiseSuppression,
    kGoogHighpassFilter,
    kGoogExperimentalAutoGainControl,
    kNumBooleanContainerIds
  };

  // This struct groups related fields or entries from
  // AudioProcessingProperties,
  // ProcessingBasedContainer::boolean_containers_, and
  // blink::WebMediaTrackConstraintSet.
  struct BooleanPropertyContainerInfo {
    BooleanContainerId index;
    BooleanConstraint ConstraintSet::*constraint_member;
    bool AudioProcessingProperties::*property_member;
  };

  static constexpr BooleanPropertyContainerInfo
      kBooleanPropertyContainerInfoMap[] = {
          {kGoogAudioMirroring, &ConstraintSet::goog_audio_mirroring,
           &AudioProcessingProperties::goog_audio_mirroring},
          {kGoogAutoGainControl, &ConstraintSet::goog_auto_gain_control,
           &AudioProcessingProperties::goog_auto_gain_control},
          {kGoogExperimentalEchoCancellation,
           &ConstraintSet::goog_experimental_echo_cancellation,
           &AudioProcessingProperties::goog_experimental_echo_cancellation},
          {kGoogNoiseSuppression, &ConstraintSet::goog_noise_suppression,
           &AudioProcessingProperties::goog_noise_suppression},
          {kGoogExperimentalNoiseSuppression,
           &ConstraintSet::goog_experimental_noise_suppression,
           &AudioProcessingProperties::goog_experimental_noise_suppression},
          {kGoogHighpassFilter, &ConstraintSet::goog_highpass_filter,
           &AudioProcessingProperties::goog_highpass_filter},
          {kGoogExperimentalAutoGainControl,
           &ConstraintSet::goog_experimental_auto_gain_control,
           &AudioProcessingProperties::goog_experimental_auto_gain_control}};

  // Private constructor intended to instantiate different variants of this
  // class based on the initial values provided. The appropriate way to
  // instantiate this class is via the three factory methods provided.
  // System echo cancellation should not be explicitly included in
  // |echo_cancellation_type|. It is added automatically based on the value of
  // |device_parameters|.
  ProcessingBasedContainer(
      ProcessingType processing_type,
      std::vector<EchoCancellationType> echo_cancellation_types,
      BoolSet goog_audio_mirroring_set,
      BoolSet goog_auto_gain_control_set,
      BoolSet goog_experimental_echo_cancellation_set,
      BoolSet goog_noise_suppression_set,
      BoolSet goog_experimental_noise_suppression_set,
      BoolSet goog_highpass_filter_set,
      BoolSet goog_experimental_auto_gain_control_set,
      IntRangeSet sample_size_range,
      IntRangeSet channels_range,
      IntRangeSet sample_rate_range,
      SourceInfo source_info,
      bool is_device_capture,
      media::AudioParameters device_parameters,
      bool is_reconfiguration_allowed)
      : processing_type_(processing_type),
        sample_size_container_(sample_size_range),
        channels_container_(channels_range),
        sample_rate_container_(sample_rate_range),
        latency_container_(
            GetAllowedLatency(processing_type, device_parameters)) {
    // If the parameters indicate that system echo cancellation is available, we
    // add such value in the allowed values for the EC type.
    if (device_parameters.effects() & media::AudioParameters::ECHO_CANCELLER ||
        device_parameters.effects() &
            media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER) {
      echo_cancellation_types.push_back(
          EchoCancellationType::kEchoCancellationSystem);
    }
    echo_cancellation_container_ = EchoCancellationContainer(
        echo_cancellation_types, source_info.HasActiveSource(),
        is_device_capture, device_parameters, source_info.properties(),
        is_reconfiguration_allowed);

    boolean_containers_[kGoogAudioMirroring] =
        BooleanContainer(goog_audio_mirroring_set);
    boolean_containers_[kGoogAutoGainControl] =
        BooleanContainer(goog_auto_gain_control_set);
    boolean_containers_[kGoogExperimentalEchoCancellation] =
        BooleanContainer(goog_experimental_echo_cancellation_set);
    boolean_containers_[kGoogNoiseSuppression] =
        BooleanContainer(goog_noise_suppression_set);
    boolean_containers_[kGoogExperimentalNoiseSuppression] =
        BooleanContainer(goog_experimental_noise_suppression_set);
    boolean_containers_[kGoogHighpassFilter] =
        BooleanContainer(goog_highpass_filter_set);
    boolean_containers_[kGoogExperimentalAutoGainControl] =
        BooleanContainer(goog_experimental_auto_gain_control_set);

    if (!source_info.HasActiveSource())
      return;

    for (size_t i = 0; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      boolean_containers_[info.index] = BooleanContainer(
          BoolSet({source_info.properties().*(info.property_member)}));
    }

    DCHECK(source_info.channels());
    channels_container_ =
        IntegerContainer(IntRangeSet::FromValue(*source_info.channels()));
    DCHECK(source_info.sample_rate() != base::nullopt);
    sample_rate_container_ =
        IntegerContainer(IntRangeSet::FromValue(*source_info.sample_rate()));
    DCHECK(source_info.latency() != base::nullopt);
    latency_container_ =
        DoubleContainer(DoubleRangeSet::FromValue(*source_info.latency()));
  }

  // The allowed latency is expressed in a range latencies in seconds.
  static const DoubleRangeSet GetAllowedLatency(
      ProcessingType processing_type,
      const media::AudioParameters& device_parameters) {
    double fallback_latency =
        static_cast<double>(blink::kFallbackAudioLatencyMs) / 1000;
    double device_latency = device_parameters.GetBufferDuration().InSecondsF();
    double allowed_latency = device_parameters.frames_per_buffer() > 0
                                 ? device_latency
                                 : fallback_latency;
    switch (processing_type) {
      case ProcessingType::kApmProcessed:
        return DoubleRangeSet::FromValue(fallback_latency);
      case ProcessingType::kNoApmProcessed:
        return DoubleRangeSet::FromValue(allowed_latency);
      case ProcessingType::kUnprocessed:
        double min_latency, max_latency;
        std::tie(min_latency, max_latency) =
            GetMinMaxLatenciesForAudioParameters(device_parameters);
        return DoubleRangeSet(min_latency, max_latency);
    }
  }

  // The priority of each processing-based container depends on the default
  // value assigned to the audio processing properties. When the value is true
  // the preference gives higher priority to the WebRTC processing.
  // On the contrary, if the value is false the preference is flipped towards
  // the option without processing.
  int GetProcessingPriority(const BooleanConstraint& ec_constraint) const {
    bool use_processing_by_default =
        echo_cancellation_container_.GetDefaultValueForAudioProperties(
            ec_constraint);

    switch (processing_type_) {
      case ProcessingType::kUnprocessed:
        return use_processing_by_default ? 1 : 3;
      case ProcessingType::kNoApmProcessed:
        return 2;
      case ProcessingType::kApmProcessed:
        return use_processing_by_default ? 3 : 1;
    }
  }

  ProcessingType processing_type_;
  std::array<BooleanContainer, kNumBooleanContainerIds> boolean_containers_;
  EchoCancellationContainer echo_cancellation_container_;
  IntegerContainer sample_size_container_;
  IntegerContainer channels_container_;
  IntegerContainer sample_rate_container_;
  DoubleContainer latency_container_;
};

constexpr ProcessingBasedContainer::BooleanPropertyContainerInfo
    ProcessingBasedContainer::kBooleanPropertyContainerInfoMap[];

// Container for the constrainable properties of a single audio device.
class DeviceContainer {
 public:
  DeviceContainer(const AudioDeviceCaptureCapability& capability,
                  bool is_device_capture,
                  bool is_reconfiguration_allowed)
      : device_parameters_(capability.Parameters()) {
    if (!capability.DeviceID().IsEmpty()) {
      device_id_container_ =
          StringContainer(StringSet({capability.DeviceID().Utf8()}));
    }

    if (!capability.GroupID().IsEmpty()) {
      group_id_container_ =
          StringContainer(StringSet({capability.GroupID().Utf8()}));
    }

    // If the device is in use, a source will be provided and all containers
    // must be initialized such that their only supported values correspond to
    // the source settings. Otherwise, the containers are initialized to contain
    // all possible values.
    SourceInfo source_info =
        InfoFromSource(capability.source(), device_parameters_.effects());

    // Three variations of the processing-based container. Each variant is
    // associated to a different type of audio processing configuration, namely
    // unprocessed, processed by WebRTC, or processed by other means.
    processing_based_containers_.push_back(
        ProcessingBasedContainer::CreateUnprocessedContainer(
            source_info, is_device_capture, device_parameters_,
            is_reconfiguration_allowed));
    processing_based_containers_.push_back(
        ProcessingBasedContainer::CreateNoApmProcessedContainer(
            source_info, is_device_capture, device_parameters_,
            is_reconfiguration_allowed));
    if (media::IsWebRtcApmInAudioServiceEnabled()) {
      processing_based_containers_.push_back(
          ProcessingBasedContainer::CreateRemoteApmProcessedContainer(
              source_info, is_device_capture, device_parameters_,
              is_reconfiguration_allowed));
    } else {
      processing_based_containers_.push_back(
          ProcessingBasedContainer::CreateApmProcessedContainer(
              source_info, is_device_capture, device_parameters_,
              is_reconfiguration_allowed));
    }

    DCHECK_EQ(processing_based_containers_.size(), 3u);

    if (source_info.type() == SourceType::kNone)
      return;

    blink::MediaStreamAudioSource* source = capability.source();
    boolean_containers_[kDisableLocalEcho] =
        BooleanContainer(BoolSet({source->disable_local_echo()}));

    boolean_containers_[kRenderToAssociatedSink] =
        BooleanContainer(BoolSet({source->RenderToAssociatedSinkEnabled()}));

#if DCHECK_IS_ON()
    for (const auto& container : boolean_containers_)
      DCHECK(!container.IsEmpty());
#endif
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* failed_constraint_name;

    failed_constraint_name =
        device_id_container_.ApplyConstraintSet(constraint_set.device_id);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        group_id_container_.ApplyConstraintSet(constraint_set.group_id);
    if (failed_constraint_name)
      return failed_constraint_name;

    for (size_t i = 0; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      failed_constraint_name =
          boolean_containers_[info.index].ApplyConstraintSet(
              constraint_set.*(info.constraint_member));
      if (failed_constraint_name)
        return failed_constraint_name;
    }

    // For each processing based container, apply the constraints and only fail
    // if all of them failed.
    for (auto it = processing_based_containers_.begin();
         it != processing_based_containers_.end();) {
      DCHECK(!it->IsEmpty());
      failed_constraint_name = it->ApplyConstraintSet(constraint_set);
      if (failed_constraint_name)
        processing_based_containers_.erase(it);
      else
        ++it;
    }
    if (processing_based_containers_.empty()) {
      DCHECK_NE(failed_constraint_name, nullptr);
      return failed_constraint_name;
    }

    return nullptr;
  }

  std::tuple<Score, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_destkop_source,
      bool should_disable_hardware_noise_suppression,
      std::string default_device_id) const {
    DCHECK(!IsEmpty());
    Score score(0.0);
    double sub_score = 0.0;

    std::string device_id;
    std::tie(sub_score, device_id) =
        device_id_container_.SelectSettingsAndScore(constraint_set.device_id,
                                                    default_device_id);
    score += sub_score;

    std::tie(sub_score, std::ignore) =
        group_id_container_.SelectSettingsAndScore(constraint_set.group_id,
                                                   std::string());
    score += sub_score;

    bool disable_local_echo;
    std::tie(sub_score, disable_local_echo) =
        boolean_containers_[kDisableLocalEcho].SelectSettingsAndScore(
            constraint_set.disable_local_echo, !is_destkop_source);
    score += sub_score;

    bool render_to_associated_sink;
    std::tie(sub_score, render_to_associated_sink) =
        boolean_containers_[kRenderToAssociatedSink].SelectSettingsAndScore(
            constraint_set.render_to_associated_sink, false);
    score += sub_score;

    // To determine which properties to use, we have to compare and compute the
    // scores of each properties set and use the best performing one. In this
    // loop we are also determining the best settings that should be applied to
    // the best performing candidate.
    Score best_score(-1.0);
    AudioProcessingProperties best_properties;
    const ProcessingBasedContainer* best_container = nullptr;
    base::Optional<int> best_requested_buffer_size;
    for (const auto& container : processing_based_containers_) {
      if (container.IsEmpty())
        continue;

      Score container_score(0.0);
      AudioProcessingProperties container_properties;
      base::Optional<int> requested_buffer_size;
      std::tie(container_score, container_properties, requested_buffer_size) =
          container.SelectSettingsAndScore(
              constraint_set, should_disable_hardware_noise_suppression,
              device_parameters_);
      if (container_score > best_score) {
        best_score = container_score;
        best_properties = container_properties;
        best_container = &container;
        best_requested_buffer_size = requested_buffer_size;
      }
    }

    DCHECK_NE(best_container, nullptr);
    score += best_score;

    // Update |properties.disable_hw_noise_suppression| depending on a related
    // experiment that can force-disable HW noise suppression.
    best_properties.disable_hw_noise_suppression =
        should_disable_hardware_noise_suppression &&
        best_properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationDisabled;

    // The score at this point can be considered complete only when the settings
    // are compared against the default device id, which is used as arbitrator
    // in case multiple candidates are available.
    return std::make_tuple(
        score, AudioCaptureSettings(
                   device_id, best_requested_buffer_size, disable_local_echo,
                   render_to_associated_sink, best_container->processing_type(),
                   best_properties));
  }

  // The DeviceContainer is considered empty if at least one of the
  // containers owned is empty.
  bool IsEmpty() const {
    DCHECK(!boolean_containers_.empty());

    for (auto& container : boolean_containers_) {
      if (container.IsEmpty())
        return true;
    }

    return device_id_container_.IsEmpty() || group_id_container_.IsEmpty();
  }

 private:
  enum BooleanContainerId {
    kDisableLocalEcho,
    kRenderToAssociatedSink,
    kNumBooleanContainerIds
  };

  // This struct groups related fields or entries from
  // DeviceContainer::boolean_containers_ and blink::WebMediaTrackConstraintSet.
  struct BooleanPropertyContainerInfo {
    BooleanContainerId index;
    BooleanConstraint ConstraintSet::*constraint_member;
  };

  static constexpr BooleanPropertyContainerInfo
      kBooleanPropertyContainerInfoMap[] = {
          {kDisableLocalEcho, &ConstraintSet::disable_local_echo},
          {kRenderToAssociatedSink, &ConstraintSet::render_to_associated_sink}};

  // Utility function to determine which version of this class should be
  // allocated depending on the |source| provided.
  static SourceInfo InfoFromSource(blink::MediaStreamAudioSource* source,
                                   int effects) {
    SourceType source_type;
    AudioProcessingProperties properties;
    auto* processed_source = ProcessedLocalAudioSource::From(source);
    base::Optional<int> channels;
    base::Optional<int> sample_rate;
    base::Optional<double> latency;

    if (!source) {
      source_type = SourceType::kNone;
    } else {
      media::AudioParameters source_parameters = source->GetAudioParameters();
      channels = source_parameters.channels();
      sample_rate = source_parameters.sample_rate();
      latency = source_parameters.GetBufferDuration().InSecondsF();
      properties = *(source->GetAudioProcessingProperties());

      if (!processed_source) {
        source_type = SourceType::kUnprocessed;
        properties.DisableDefaultProperties();

        // It is possible, however, that the HW echo canceller is enabled. In
        // such case the property for echo cancellation type should be updated
        // accordingly.
        if (effects & media::AudioParameters::ECHO_CANCELLER) {
          properties.echo_cancellation_type =
              EchoCancellationType::kEchoCancellationSystem;
        }
      } else {
        source_type = properties.EchoCancellationIsWebRtcProvided()
                          ? SourceType::kApmProcessed
                          : SourceType::kNoApmProcessed;
        properties = processed_source->audio_processing_properties();
      }
    }

    return SourceInfo(source_type, properties, channels, sample_rate, latency);
  }

  media::AudioParameters device_parameters_;
  StringContainer device_id_container_;
  StringContainer group_id_container_;
  std::array<BooleanContainer, kNumBooleanContainerIds> boolean_containers_;
  std::vector<ProcessingBasedContainer> processing_based_containers_;
};

constexpr DeviceContainer::BooleanPropertyContainerInfo
    DeviceContainer::kBooleanPropertyContainerInfoMap[];

// This class represents a set of possible candidate settings.  The
// SelectSettings algorithm starts with a set containing all possible candidates
// based on system/hardware capabilities and/or allowed values for supported
// properties. The set is then reduced progressively as the basic and advanced
// constraint sets are applied. In the end, if the set of candidates is empty,
// SelectSettings fails. If not, the ideal values (if any) or tie breaker rules
// are used to select the final settings based on the candidates that survived
// the application of the constraint sets. This class is implemented as a
// collection of more specific sets for the various supported properties. If any
// of the specific sets is empty, the whole CandidatesContainer is considered
// empty as well.
class CandidatesContainer {
 public:
  CandidatesContainer(const AudioDeviceCaptureCapabilities& capabilities,
                      std::string& media_stream_source,
                      std::string& default_device_id,
                      bool is_reconfiguration_allowed)
      : default_device_id_(default_device_id) {
    for (const auto& capability : capabilities) {
      devices_.emplace_back(capability, media_stream_source.empty(),
                            is_reconfiguration_allowed);
      DCHECK(!devices_.back().IsEmpty());
    }
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* latest_failed_constraint_name = nullptr;
    for (auto it = devices_.begin(); it != devices_.end();) {
      DCHECK(!it->IsEmpty());
      auto* failed_constraint_name = it->ApplyConstraintSet(constraint_set);
      if (failed_constraint_name) {
        latest_failed_constraint_name = failed_constraint_name;
        devices_.erase(it);
      } else {
        ++it;
      }
    }
    return IsEmpty() ? latest_failed_constraint_name : nullptr;
  }

  std::tuple<Score, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_desktop_source,
      bool should_disable_hardware_noise_suppression) const {
    DCHECK(!IsEmpty());
    // Make a copy of the settings initially provided, to track the default
    // settings.
    AudioCaptureSettings best_settings;
    Score best_score(-1.0);
    for (const auto& candidate : devices_) {
      Score score(0.0);
      AudioCaptureSettings settings;
      std::tie(score, settings) = candidate.SelectSettingsAndScore(
          constraint_set, is_desktop_source,
          should_disable_hardware_noise_suppression, default_device_id_);

      score += default_device_id_ == settings.device_id();
      if (score > best_score) {
        best_score = score;
        best_settings = std::move(settings);
      }
    }
    return std::make_tuple(best_score, best_settings);
  }

  bool IsEmpty() const { return devices_.empty(); }

 private:
  std::string default_device_id_;
  std::vector<DeviceContainer> devices_;
};

std::string GetMediaStreamSource(const WebMediaConstraints& constraints) {
  std::string source;
  if (constraints.Basic().media_stream_source.HasIdeal() &&
      constraints.Basic().media_stream_source.Ideal().size() > 0) {
    source = constraints.Basic().media_stream_source.Ideal()[0].Utf8();
  }
  if (constraints.Basic().media_stream_source.HasExact() &&
      constraints.Basic().media_stream_source.Exact().size() > 0) {
    source = constraints.Basic().media_stream_source.Exact()[0].Utf8();
  }

  return source;
}

}  // namespace

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability()
    : parameters_(media::AudioParameters::UnavailableDeviceParams()) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    blink::MediaStreamAudioSource* source)
    : source_(source) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    String device_id,
    String group_id,
    const media::AudioParameters& parameters)
    : device_id_(std::move(device_id)),
      group_id_(std::move(group_id)),
      parameters_(parameters) {
  DCHECK(!device_id_.IsEmpty());
}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    const AudioDeviceCaptureCapability& other) = default;

String AudioDeviceCaptureCapability::DeviceID() const {
  return source_ ? String(source_->device().id.data()) : device_id_;
}

String AudioDeviceCaptureCapability::GroupID() const {
  return source_ && source_->device().group_id
             ? String(source_->device().group_id->data())
             : group_id_;
}

const media::AudioParameters& AudioDeviceCaptureCapability::Parameters() const {
  return source_ ? source_->device().input : parameters_;
}

AudioCaptureSettings SelectSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const blink::WebMediaConstraints& constraints,
    bool should_disable_hardware_noise_suppression,
    bool is_reconfiguration_allowed) {
  if (capabilities.IsEmpty())
    return AudioCaptureSettings();

  std::string media_stream_source = GetMediaStreamSource(constraints);
  std::string default_device_id;
  bool is_device_capture = media_stream_source.empty();
  if (is_device_capture)
    default_device_id = capabilities.begin()->DeviceID().Utf8();

  CandidatesContainer candidates(capabilities, media_stream_source,
                                 default_device_id, is_reconfiguration_allowed);
  DCHECK(!candidates.IsEmpty());

  auto* failed_constraint_name =
      candidates.ApplyConstraintSet(constraints.Basic());
  if (failed_constraint_name)
    return AudioCaptureSettings(failed_constraint_name);

  for (const auto& advanced_set : constraints.Advanced()) {
    CandidatesContainer copy = candidates;
    failed_constraint_name = candidates.ApplyConstraintSet(advanced_set);
    if (failed_constraint_name)
      candidates = std::move(copy);
  }
  DCHECK(!candidates.IsEmpty());

  // Score is ignored as it is no longer needed.
  AudioCaptureSettings settings;
  std::tie(std::ignore, settings) = candidates.SelectSettingsAndScore(
      constraints.Basic(),
      media_stream_source == blink::kMediaStreamSourceDesktop,
      should_disable_hardware_noise_suppression);

  return settings;
}

AudioCaptureSettings SelectSettingsAudioCapture(
    blink::MediaStreamAudioSource* source,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(source);
  if (source->device().type !=
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      source->device().type !=
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
      source->device().type !=
          blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE) {
    return AudioCaptureSettings();
  }

  std::string media_stream_source = GetMediaStreamSource(constraints);
  if (source->device().type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      !media_stream_source.empty()) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }

  if (source->device().type ==
          blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != blink::kMediaStreamSourceTab) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }
  if (source->device().type ==
          blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != blink::kMediaStreamSourceSystem &&
      media_stream_source != blink::kMediaStreamSourceDesktop) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }

  AudioDeviceCaptureCapabilities capabilities = {
      AudioDeviceCaptureCapability(source)};
  bool should_disable_hardware_noise_suppression =
      !(source->device().input.effects() &
        media::AudioParameters::NOISE_SUPPRESSION);

  return SelectSettingsAudioCapture(capabilities, constraints,
                                    should_disable_hardware_noise_suppression);
}

std::tuple<int, int> GetMinMaxBufferSizesForAudioParameters(
    const media::AudioParameters& parameters) {
  const int default_buffer_size = parameters.frames_per_buffer();
  DCHECK_GT(default_buffer_size, 0);

  const base::Optional<media::AudioParameters::HardwareCapabilities>
      hardware_capabilities = parameters.hardware_capabilities();

  // Only support platforms where we have both fixed min and max buffer size
  // values in order to simplify comparison logic.
  DCHECK(!hardware_capabilities ||
         (hardware_capabilities &&
          // Windows returns a HardwareCapabilities with both values set to 0 if
          // they're unknown rather than returning null.
          ((hardware_capabilities->min_frames_per_buffer == 0 &&
            hardware_capabilities->max_frames_per_buffer == 0) ||
           (hardware_capabilities->min_frames_per_buffer > 0 &&
            hardware_capabilities->max_frames_per_buffer > 0))))
      << "Variable input latency requires both a min and max to be set";

  return (hardware_capabilities &&
          hardware_capabilities->min_frames_per_buffer > 0 &&
          hardware_capabilities->max_frames_per_buffer > 0)
             ? std::make_tuple(hardware_capabilities->min_frames_per_buffer,
                               hardware_capabilities->max_frames_per_buffer)
             : std::make_tuple(default_buffer_size, default_buffer_size);
}

std::tuple<double, double> GetMinMaxLatenciesForAudioParameters(
    const media::AudioParameters& parameters) {
  int min_buffer_size, max_buffer_size;
  std::tie(min_buffer_size, max_buffer_size) =
      GetMinMaxBufferSizesForAudioParameters(parameters);

  // Doing the microseconds conversion to match what is done in
  // AudioParameters::GetBufferDuration() so that values reported to the user
  // are truncated consistently to the microseconds decimal place.
  return std::make_tuple(
      base::TimeDelta::FromMicroseconds(
          static_cast<int64_t>(min_buffer_size *
                               base::Time::kMicrosecondsPerSecond /
                               static_cast<float>(parameters.sample_rate())))
          .InSecondsF(),
      base::TimeDelta::FromMicroseconds(
          static_cast<int64_t>(max_buffer_size *
                               base::Time::kMicrosecondsPerSecond /
                               static_cast<float>(parameters.sample_rate())))
          .InSecondsF());
}

}  // namespace blink
