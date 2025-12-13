// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/webrtc/constants.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_string.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_audio.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using blink::AudioCaptureSettings;
using blink::AudioProcessingProperties;
using ConstraintSet = MediaTrackConstraintSetPlatform;
using BooleanConstraint = blink::BooleanConstraint;
using VoiceIsolationType = AudioProcessingProperties::VoiceIsolationType;
using ProcessingType = AudioCaptureSettings::ProcessingType;
using StringConstraint = blink::StringConstraint;

template <class T>
using NumericRangeSet = blink::media_constraints::NumericRangeSet<T>;

namespace {
using BoolSet = blink::media_constraints::DiscreteSet<bool>;
using DoubleRangeSet = blink::media_constraints::NumericRangeSet<double>;
using VoiceIsolationTypeSet =
    blink::media_constraints::DiscreteSet<VoiceIsolationType>;
using IntRangeSet = blink::media_constraints::NumericRangeSet<int>;
using StringSet = blink::media_constraints::DiscreteSet<std::string>;
using EchoCancellationModeSet =
    blink::media_constraints::DiscreteSet<EchoCancellationMode>;

// The sample size is set to 16 due to the Signed-16 format representation.
int32_t GetSampleSize() {
  return media::SampleFormatToBitsPerChannel(media::kSampleFormatS16);
}

enum class AudioCaptureApi {
  // Standard microphone capture using getUserMedia
  kGumMicrophone,
  // All forms of screen capture via extension APIs, including tab, window and
  // desktop/system capture
  kExtensionScreenShare,
  // All other capture, including getDisplayMedia
  kOther,
};

AudioCaptureApi GetAudioCaptureApi(mojom::blink::MediaStreamType stream_type,
                                   const std::string& media_stream_source) {
  if (stream_type == mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
    return AudioCaptureApi::kGumMicrophone;
  }
  return media_stream_source.empty() ? AudioCaptureApi::kOther
                                     : AudioCaptureApi::kExtensionScreenShare;
}

// Returns true if kAll and kRemoteOnly should be added as choices for a given
// device if supported.
bool ShouldSupportExtendedEchoCancellationModes(AudioCaptureApi capture_type) {
  return RuntimeEnabledFeatures::GetUserMediaEchoCancellationModesEnabled() &&
         (capture_type == AudioCaptureApi::kGumMicrophone);
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
    kRemoteOnly = 2,
    kAll = 3,
    kBrowserDecides = 4,
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

// Information regarding an active source, if that exists.
class SourceInfo {
 public:
  static std::optional<SourceInfo> FromSource(
      blink::MediaStreamAudioSource* source) {
    if (!source) {
      return std::nullopt;
    }

    media::AudioParameters source_parameters = source->GetAudioParameters();
    std::optional<AudioProcessingProperties> properties =
        source->GetAudioProcessingProperties();
    CHECK(properties);

    return SourceInfo(*properties, source_parameters.channels(),
                      source_parameters.sample_rate(),
                      source_parameters.GetBufferDuration().InSecondsF());
  }

  const AudioProcessingProperties& properties() const { return properties_; }
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }
  double latency() const { return latency_; }

 private:
  SourceInfo(const AudioProcessingProperties& properties,
             int channels,
             int sample_rate,
             double latency)
      : properties_(properties),
        channels_(std::move(channels)),
        sample_rate_(std::move(sample_rate)),
        latency_(latency) {}

  const AudioProcessingProperties properties_;
  const int channels_;
  const int sample_rate_;
  const double latency_;
};

// Container for each independent boolean constrainable property.
class BooleanContainer {
 public:
  explicit BooleanContainer(BoolSet allowed_values = BoolSet())
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
      for (const String& ideal_candidate : constraint.Ideal()) {
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
class NumericRangeSetContainer {
 public:
  explicit NumericRangeSetContainer(
      NumericRangeSet<T> allowed_values = NumericRangeSet<T>())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const C& constraint) {
    auto constraint_set = NumericRangeSet<T>::FromConstraint(constraint);
    allowed_values_ = allowed_values_.Intersection(constraint_set);
    return IsEmpty() ? constraint.GetName() : nullptr;
  }

  // This function will return a fitness with the associated setting.
  // The setting will be the ideal value, if such value is provided and
  // admitted, or the closest value to it.
  // When no ideal is available and |default_setting| is provided, the setting
  // will be |default_setting| or the closest value to it.
  // When |default_setting| is **not** provided, the setting will be a value iff
  // |allowed_values_| contains only a single value, otherwise std::nullopt is
  // returned to signal that it was not possible to make a decision.
  std::tuple<double, std::optional<T>> SelectSettingsAndScore(
      const C& constraint,
      const std::optional<T>& default_setting = std::nullopt) const {
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

    return std::make_tuple(0.0, std::nullopt);
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

using IntegerRangeContainer =
    NumericRangeSetContainer<int, blink::LongConstraint>;
using DoubleRangeContainer =
    NumericRangeSetContainer<double, blink::DoubleConstraint>;

// Container for numeric constrainable properties that allow a fixed set of
// values.
template <class T, class C>
class NumericDiscreteSetContainer {
 public:
  // It's the responsibility of the caller to ensure there are no repeated
  // values.
  explicit NumericDiscreteSetContainer(Vector<T> allowed_values)
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const C& constraint) {
    auto constraint_set = NumericRangeSet<T>::FromConstraint(constraint);
    auto to_remove = std::ranges::remove_if(
        allowed_values_, [&constraint_set](const auto& t) {
          return !constraint_set.Contains(t);
        });
    allowed_values_.erase(to_remove.begin(), to_remove.end());

    return IsEmpty() ? constraint.GetName() : nullptr;
  }

  // This function will return a fitness with the associated setting. The
  // setting will be the ideal value, if ideal is provided and
  // allowed, or the closest value to it (using fitness distance).
  // When no ideal is available and |default_setting| is provided, the setting
  // will be |default_setting| or the closest value to it (using fitness
  // distance).
  // When |default_setting| is **not** provided, the setting will be a value iff
  // |allowed_values_| contains only a single value, otherwise std::nullopt is
  // returned to signal that it was not possible to make a decision.
  std::tuple<double, std::optional<T>> SelectSettingsAndScore(
      const C& constraint,
      const std::optional<T>& default_setting = std::nullopt) const {
    DCHECK(!IsEmpty());

    if (constraint.HasIdeal()) {
      if (allowed_values_.Contains(constraint.Ideal()))
        return std::make_tuple(1.0, constraint.Ideal());

      T value = SelectClosestValueTo(constraint.Ideal());
      double fitness =
          1.0 - NumericConstraintFitnessDistance(value, constraint.Ideal());
      return std::make_tuple(fitness, value);
    }

    if (default_setting) {
      if (allowed_values_.Contains(*default_setting))
        return std::make_tuple(0.0, *default_setting);

      // If the default value provided is not contained, select the value
      // closest to it.
      return std::make_tuple(0.0, SelectClosestValueTo(*default_setting));
    }

    if (allowed_values_.size() == 1) {
      return std::make_tuple(0.0, *allowed_values_.begin());
    }

    return std::make_tuple(0.0, std::nullopt);
  }

  bool IsEmpty() const { return allowed_values_.empty(); }

 private:
  T SelectClosestValueTo(T target) const {
    DCHECK(!IsEmpty());
    T best_value = *allowed_values_.begin();
    double best_distance = HUGE_VAL;
    for (auto value : allowed_values_) {
      double distance = blink::NumericConstraintFitnessDistance(value, target);
      if (distance < best_distance) {
        best_value = value;
        best_distance = distance;
      }
    }
    return best_value;
  }

  Vector<T> allowed_values_;
};

using IntegerDiscreteContainer =
    NumericDiscreteSetContainer<int, blink::LongConstraint>;

EchoCancellationModeSet EchoCancellationModeSetFromConstraint(
    const BooleanOrStringConstraint& constraint,
    AudioCaptureApi api) {
  if (!constraint.HasExact()) {
    return EchoCancellationModeSet::UniversalSet();
  }
  if (constraint.HasExactBoolean()) {
    return EchoCancellationModeSet({constraint.ExactBoolean()
                                        ? EchoCancellationMode::kBrowserDecides
                                        : EchoCancellationMode::kDisabled});
  }
  if (ShouldSupportExtendedEchoCancellationModes(api)) {
    String mode = constraint.ExactString();
    if (mode == kEchoCancellationModeRemoteOnly) {
      return EchoCancellationModeSet({EchoCancellationMode::kRemoteOnly});
    }
    if (mode == kEchoCancellationModeAll) {
      return EchoCancellationModeSet({EchoCancellationMode::kAll});
    }
  }
  return EchoCancellationModeSet::EmptySet();
}

std::optional<EchoCancellationMode> IdealEchoCancellationModeFromConstraint(
    const BooleanOrStringConstraint& constraint,
    AudioCaptureApi api) {
  if (!constraint.HasIdeal()) {
    return std::nullopt;
  }
  if (constraint.HasIdealBoolean()) {
    return constraint.IdealBoolean() ? EchoCancellationMode::kBrowserDecides
                                     : EchoCancellationMode::kDisabled;
  }
  if (ShouldSupportExtendedEchoCancellationModes(api)) {
    CHECK(constraint.HasIdealString());
    String mode = constraint.IdealString();
    if (mode == kEchoCancellationModeRemoteOnly) {
      return EchoCancellationMode::kRemoteOnly;
    }
    if (mode == kEchoCancellationModeAll) {
      return EchoCancellationMode::kAll;
    }
  }
  return std::nullopt;
}

bool IsEnabledEchoCancellationMode(EchoCancellationMode ec_mode) {
  return ec_mode != EchoCancellationMode::kDisabled;
}

// Container to manage the properties related to echo cancellation.
class EchoCancellationContainer {
 public:
  // Default constructor intended to temporarily create an empty object.
  EchoCancellationContainer()
      : ec_allowed_values_(EchoCancellationModeSet::EmptySet()),
        device_parameters_(media::AudioParameters::UnavailableDeviceParams()),
        api_(AudioCaptureApi::kGumMicrophone) {}

  EchoCancellationContainer(Vector<EchoCancellationMode> allowed_values,
                            std::optional<SourceInfo> source_info,
                            AudioCaptureApi api,
                            media::AudioParameters device_parameters,
                            bool is_reconfiguration_allowed)
      : ec_allowed_values_(EchoCancellationModeSet(std::move(allowed_values))),
        device_parameters_(device_parameters),
        api_(api) {
    if (!source_info) {
      return;
    }

    // If HW echo cancellation is used, reconfiguration is not always supported
    // and only the current values are allowed. Otherwise, allow all possible
    // values for echo cancellation.
    // TODO(crbug.com/1481032): Consider extending to other platforms. It is not
    // known at the moment what OSes support this behavior.
    const bool is_aec_reconfiguration_supported =
#if BUILDFLAG(IS_CHROMEOS)
        // ChromeOS is currently the only platform where we have confirmed
        // support for simultaneous streams with and without hardware AEC on the
        // same device.
        true;
#else
        // Allowing it when the system echo cancellation is enforced via flag,
        // for evaluation purposes.
        media::IsSystemEchoCancellationEnforced() ||
        (source_info->properties().echo_cancellation_mode !=
             EchoCancellationMode::kDisabled &&
         !EchoCanceller::From(source_info->properties(),
                              device_parameters.effects())
              .IsPlatformProvided());
#endif
    if (is_reconfiguration_allowed && is_aec_reconfiguration_supported) {
      return;
    }

    ec_allowed_values_ = EchoCancellationModeSet(
        {source_info->properties().echo_cancellation_mode});
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    // Convert the constraints into discrete sets.
    EchoCancellationModeSet ec_set = EchoCancellationModeSetFromConstraint(
        constraint_set.echo_cancellation, api_);

    // Apply echoCancellation constraint.
    ec_allowed_values_ = ec_allowed_values_.Intersection(ec_set);
    return IsEmpty() ? constraint_set.echo_cancellation.GetName() : nullptr;
  }

  std::tuple<Score, EchoCancellationMode> SelectSettingsAndScore(
      const ConstraintSet& constraint_set) const {
    EchoCancellationMode selected_ec_mode = SelectBestEcMode(constraint_set);
    double fitness =
        Fitness(selected_ec_mode, constraint_set.echo_cancellation);
    Score score(fitness);
    score.set_ec_mode_score(GetEcModeScore(selected_ec_mode));
    return std::make_tuple(score, selected_ec_mode);
  }

  bool IsEmpty() const { return ec_allowed_values_.IsEmpty(); }

  // Audio-processing properties are disabled by default for content capture,
  // or if the |echo_cancellation| constraint is false.
  void UpdateDefaultValues(
      const BooleanOrStringConstraint& echo_cancellation_constraint,
      AudioProcessingProperties* properties) const {
    bool default_audio_processing_value =
        GetDefaultValueForAudioProperties(echo_cancellation_constraint);

    properties->auto_gain_control &= default_audio_processing_value;

    properties->noise_suppression &= default_audio_processing_value;
    properties->voice_isolation = VoiceIsolationType::kVoiceIsolationDefault;
  }

  bool GetDefaultValueForAudioProperties(
      const BooleanOrStringConstraint& ec_constraint) const {
    std::optional<EchoCancellationMode> ideal_mode =
        IdealEchoCancellationModeFromConstraint(ec_constraint, api_);
    if (ideal_mode && ec_allowed_values_.Contains(*ideal_mode)) {
      return (api_ != AudioCaptureApi::kExtensionScreenShare) &&
             IsEnabledEchoCancellationMode(*ideal_mode);
    }

    if (ec_allowed_values_.Contains(EchoCancellationMode::kBrowserDecides) ||
        ec_allowed_values_.Contains(EchoCancellationMode::kAll) ||
        ec_allowed_values_.Contains(EchoCancellationMode::kRemoteOnly)) {
      return api_ != AudioCaptureApi::kExtensionScreenShare;
    }

    return false;
  }

 private:
  static Score::EcModeScore GetEcModeScore(EchoCancellationMode ec_type) {
    switch (ec_type) {
      case EchoCancellationMode::kDisabled:
        return Score::EcModeScore::kDisabled;
      case EchoCancellationMode::kBrowserDecides:
        return Score::EcModeScore::kBrowserDecides;
      case EchoCancellationMode::kAll:
        return Score::EcModeScore::kAll;
      case EchoCancellationMode::kRemoteOnly:
        return Score::EcModeScore::kRemoteOnly;
    }
  }

  EchoCancellationMode SelectBestEcMode(
      const ConstraintSet& constraint_set) const {
    CHECK(!IsEmpty());

    // Try to use an ideal candidate, if supplied.
    std::optional<EchoCancellationMode> ideal_mode =
        IdealEchoCancellationModeFromConstraint(
            constraint_set.echo_cancellation, api_);
    if (ideal_mode && ec_allowed_values_.Contains(*ideal_mode)) {
      return *ideal_mode;
    }

    // If no ideal could be selected and the set contains only one value, pick
    // that one.
    if (ec_allowed_values_.elements().size() == 1) {
      return ec_allowed_values_.FirstElement();
    }

    switch (api_) {
      case AudioCaptureApi::kGumMicrophone:
      case AudioCaptureApi::kOther:
        if (ec_allowed_values_.Contains(
                EchoCancellationMode::kBrowserDecides)) {
          return EchoCancellationMode::kBrowserDecides;
        }
        if (RuntimeEnabledFeatures::
                GetUserMediaEchoCancellationModesEnabled()) {
          if (ec_allowed_values_.Contains(EchoCancellationMode::kAll)) {
            return EchoCancellationMode::kAll;
          }
          if (ec_allowed_values_.Contains(EchoCancellationMode::kRemoteOnly)) {
            return EchoCancellationMode::kRemoteOnly;
          }
        }
        CHECK(ec_allowed_values_.Contains(EchoCancellationMode::kDisabled));
        return EchoCancellationMode::kDisabled;

      case AudioCaptureApi::kExtensionScreenShare:
        if (ec_allowed_values_.Contains(EchoCancellationMode::kDisabled)) {
          return EchoCancellationMode::kDisabled;
        }
        CHECK(
            ec_allowed_values_.Contains(EchoCancellationMode::kBrowserDecides));
        return EchoCancellationMode::kBrowserDecides;
    }
  }

  // This function computes the fitness score of the given |ec_mode|. The
  // fitness is determined by the ideal values of |ec_constraint|. If |ec_mode|
  // satisfies the constraint, the fitness score results in a value of 1, and 0
  // otherwise. If no ideal value is specified, the fitness is 1.
  double Fitness(EchoCancellationMode ec_mode,
                 const BooleanOrStringConstraint& ec_constraint) const {
    std::optional<EchoCancellationMode> ideal_mode =
        IdealEchoCancellationModeFromConstraint(ec_constraint, api_);
    if (!ideal_mode) {
      return 1.0;
    }

    switch (*ideal_mode) {
      case EchoCancellationMode::kBrowserDecides:
        return ec_mode != EchoCancellationMode::kDisabled;
      case EchoCancellationMode::kDisabled:
        return ec_mode == EchoCancellationMode::kDisabled;
      case EchoCancellationMode::kAll:
        return ec_mode == EchoCancellationMode::kAll;
      case EchoCancellationMode::kRemoteOnly:
        return ec_mode == EchoCancellationMode::kRemoteOnly;
    }
  }

  EchoCancellationModeSet ec_allowed_values_;
  media::AudioParameters device_parameters_;
  AudioCaptureApi api_;
};

class AutoGainControlContainer {
 public:
  explicit AutoGainControlContainer(BoolSet allowed_values = BoolSet())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    BoolSet agc_set = blink::media_constraints::BoolSetFromConstraint(
        constraint_set.auto_gain_control);
    // Apply autoGainControl constraint.
    allowed_values_ = allowed_values_.Intersection(agc_set);
    return IsEmpty() ? constraint_set.auto_gain_control.GetName() : nullptr;
  }

  std::tuple<double, bool> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool default_setting) const {
    BooleanConstraint agc_constraint = constraint_set.auto_gain_control;

    if (agc_constraint.HasIdeal()) {
      bool agc_ideal = agc_constraint.Ideal();
      if (allowed_values_.Contains(agc_ideal))
        return std::make_tuple(1.0, agc_ideal);
    }

    if (allowed_values_.is_universal()) {
      return std::make_tuple(0.0, default_setting);
    }

    return std::make_tuple(0.0, allowed_values_.FirstElement());
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  BoolSet allowed_values_;
};

class VoiceIsolationContainer {
 public:
  // Default constructor intended to temporarily create an empty object.
  explicit VoiceIsolationContainer(BoolSet allowed_values = BoolSet())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    BoolSet voice_isolation_set =
        blink::media_constraints::BoolSetFromConstraint(
            constraint_set.voice_isolation);
    // Apply voice isolation constraint.
    allowed_values_ = allowed_values_.Intersection(voice_isolation_set);
    return IsEmpty() ? constraint_set.voice_isolation.GetName() : nullptr;
  }

  std::tuple<double, VoiceIsolationType> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      VoiceIsolationType default_setting) const {
    BooleanConstraint voice_isolation_constraint =
        constraint_set.voice_isolation;

    if (voice_isolation_constraint.HasIdeal()) {
      VoiceIsolationType voice_isolation_type_ideal =
          voice_isolation_constraint.Ideal()
              ? VoiceIsolationType::kVoiceIsolationEnabled
              : VoiceIsolationType::kVoiceIsolationDisabled;

      return std::make_tuple(1.0, voice_isolation_type_ideal);
    }

    if (allowed_values_.is_universal()) {
      return std::make_tuple(0.0, default_setting);
    }

    VoiceIsolationType voice_isolation_first =
        allowed_values_.FirstElement()
            ? VoiceIsolationType::kVoiceIsolationEnabled
            : VoiceIsolationType::kVoiceIsolationDisabled;

    return std::make_tuple(0.0, voice_isolation_first);
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  BoolSet allowed_values_;
};

Vector<int> GetApmSupportedChannels(
    const media::AudioParameters& device_params) {
  Vector<int> result;
  // APM always supports mono output;
  result.push_back(1);
  const int channels = device_params.channels();
  if (channels > 1)
    result.push_back(channels);
  return result;
}

// This container represents the supported audio settings for a given type of
// audio source. In practice, there are three types of sources: processed using
// APM, processed without APM, and unprocessed. Processing using APM has two
// flavors: one for the systems where audio processing is done in the renderer,
// another for the systems where audio processing is done in the audio service.
class ProcessingBasedContainer {
 public:
  // Creates an instance of ProcessingBasedContainer for the WebRTC processed
  // source type. The source type allows (a) any type of echo cancellation,
  // though the system echo cancellation type depends on the availability of the
  // related |parameters.effects()|, and (b) any combination of processing
  // properties settings.
  static ProcessingBasedContainer CreateApmProcessedContainer(
      std::optional<SourceInfo> source_info,
      AudioCaptureApi api,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    Vector<EchoCancellationMode> echo_cancellation_modes;
    echo_cancellation_modes.push_back(EchoCancellationMode::kBrowserDecides);
    if (ShouldSupportExtendedEchoCancellationModes(api)) {
      // kRemoteOnly is not supported on mobile platforms.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      echo_cancellation_modes.push_back(EchoCancellationMode::kRemoteOnly);
#endif
      if (EchoCanceller::IsSystemWideAecAvailable(
              device_parameters.effects())) {
        echo_cancellation_modes.push_back(EchoCancellationMode::kAll);
      }
    }
    echo_cancellation_modes.push_back(EchoCancellationMode::kDisabled);
    return ProcessingBasedContainer(
        ProcessingType::kApmProcessed, std::move(echo_cancellation_modes),
        /*auto_gain_control_set=*/BoolSet(),
        /*noise_suppression_set=*/BoolSet(),
        /*voice_isolation_set=*/BoolSet(),
        /*sample_size_range=*/IntRangeSet::FromValue(GetSampleSize()),
        /*channels_set=*/GetApmSupportedChannels(device_parameters),
        /*sample_rate_range=*/
        IntRangeSet::FromValue(media::WebRtcAudioProcessingSampleRateHz()),
        source_info, api, device_parameters, is_reconfiguration_allowed);
  }

  // Creates an instance of ProcessingBasedContainer for the processed source
  // type. The source type allows (a) either system echo cancellation, if
  // allowed by the |parameters.effects()|, or none, while (b) all other
  // processing properties settings cannot be enabled.
  static ProcessingBasedContainer CreateNoApmProcessedContainer(
      std::optional<SourceInfo> source_info,
      AudioCaptureApi api,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kNoApmProcessed, {EchoCancellationMode::kDisabled},
        /*auto_gain_control_set=*/BoolSet({false}),
        /*noise_suppression_set=*/BoolSet({false}),
        /*voice_isolation_set=*/BoolSet(),
        /*sample_size_range=*/IntRangeSet::FromValue(GetSampleSize()),
        /*channels_set=*/{device_parameters.channels()},
        /*sample_rate_range=*/
        IntRangeSet::FromValue(device_parameters.sample_rate()), source_info,
        api, device_parameters, is_reconfiguration_allowed);
  }

  // Creates an instance of ProcessingBasedContainer for the unprocessed source
  // type. The source type allows (a) either system echo cancellation, if
  // allowed by the |parameters.effects()|, or none, while (c) all processing
  // properties settings cannot be enabled.
  static ProcessingBasedContainer CreateUnprocessedContainer(
      std::optional<SourceInfo> source_info,
      AudioCaptureApi api,
      const media::AudioParameters& device_parameters,
      bool is_reconfiguration_allowed) {
    return ProcessingBasedContainer(
        ProcessingType::kUnprocessed, {EchoCancellationMode::kDisabled},
        /*auto_gain_control_set=*/BoolSet({false}),
        /*noise_suppression_set=*/BoolSet({false}),
        /*voice_isolation_set=*/BoolSet({false}),
        /*sample_size_range=*/IntRangeSet::FromValue(GetSampleSize()),
        /*channels_set=*/{device_parameters.channels()},
        /*sample_rate_range=*/
        IntRangeSet::FromValue(device_parameters.sample_rate()), source_info,
        api, device_parameters, is_reconfiguration_allowed);
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* failed_constraint_name = nullptr;

    failed_constraint_name =
        echo_cancellation_container_.ApplyConstraintSet(constraint_set);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        auto_gain_control_container_.ApplyConstraintSet(constraint_set);
    if (failed_constraint_name)
      return failed_constraint_name;

    failed_constraint_name =
        voice_isolation_container_.ApplyConstraintSet(constraint_set);
    if (failed_constraint_name) {
      return failed_constraint_name;
    }

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

    failed_constraint_name = noise_suppression_container_.ApplyConstraintSet(
        constraint_set.noise_suppression);
    if (failed_constraint_name) {
      return failed_constraint_name;
    }

    return failed_constraint_name;
  }

  std::tuple<Score,
             AudioProcessingProperties,
             std::optional<int> /* requested_buffer_size */,
             int /* num_channels */>
  SelectSettingsAndScore(const ConstraintSet& constraint_set,
                         const media::AudioParameters& parameters) const {
    DCHECK(!IsEmpty());

    Score score(0.0);
    double sub_score(0.0);

    std::tie(sub_score, std::ignore) =
        sample_size_container_.SelectSettingsAndScore(
            constraint_set.sample_size, GetSampleSize());
    score += sub_score;

    std::optional<int> num_channels;
    std::tie(sub_score, num_channels) =
        channels_container_.SelectSettingsAndScore(constraint_set.channel_count,
                                                   /*default_setting=*/1);
    DCHECK(num_channels);
    score += sub_score;

    std::optional<int> sample_size;
    std::tie(sub_score, sample_size) =
        sample_rate_container_.SelectSettingsAndScore(
            constraint_set.sample_rate);
    DCHECK(sample_size != std::nullopt);
    score += sub_score;

    std::optional<double> latency;
    std::tie(sub_score, latency) =
        latency_container_.SelectSettingsAndScore(constraint_set.latency);
    score += sub_score;

    // Only request an explicit change to the buffer size for the unprocessed
    // container, and only if it's based on a specific user constraint.
    std::optional<int> requested_buffer_size;
    if (processing_type_ == ProcessingType::kUnprocessed && latency &&
        !constraint_set.latency.IsUnconstrained()) {
      auto [min_buffer_size, max_buffer_size] =
          GetMinMaxBufferSizesForAudioParameters(parameters);
      requested_buffer_size = media::AudioLatency::GetExactBufferSize(
          base::Seconds(*latency), parameters.sample_rate(),
          parameters.frames_per_buffer(), min_buffer_size, max_buffer_size,
          max_buffer_size);
    }

    AudioProcessingProperties properties;
    Score ec_score(0.0);
    std::tie(ec_score, properties.echo_cancellation_mode) =
        echo_cancellation_container_.SelectSettingsAndScore(constraint_set);
    score += ec_score;

    // Update the default settings for each audio-processing properties
    // according to |echo_cancellation| and whether the source considered is
    // device capture.
    echo_cancellation_container_.UpdateDefaultValues(
        constraint_set.echo_cancellation, &properties);

    std::tie(sub_score, properties.auto_gain_control) =
        auto_gain_control_container_.SelectSettingsAndScore(
            constraint_set, properties.auto_gain_control);
    score += sub_score;

    std::tie(sub_score, properties.voice_isolation) =
        voice_isolation_container_.SelectSettingsAndScore(
            constraint_set, properties.voice_isolation);
    score += sub_score;

    std::tie(sub_score, properties.noise_suppression) =
        noise_suppression_container_.SelectSettingsAndScore(
            constraint_set.noise_suppression, properties.noise_suppression);
    score += sub_score;

    score.set_processing_priority(
        GetProcessingPriority(constraint_set.echo_cancellation));
    return std::make_tuple(score, properties, requested_buffer_size,
                           *num_channels);
  }

  // The ProcessingBasedContainer is considered empty if at least one of the
  // containers owned by it is empty.
  bool IsEmpty() const {
    return echo_cancellation_container_.IsEmpty() ||
           auto_gain_control_container_.IsEmpty() ||
           noise_suppression_container_.IsEmpty() ||
           sample_size_container_.IsEmpty() || channels_container_.IsEmpty() ||
           sample_rate_container_.IsEmpty() || latency_container_.IsEmpty();
  }

  ProcessingType processing_type() const { return processing_type_; }

 private:
  // Private constructor intended to instantiate different variants of this
  // class based on the initial values provided. The appropriate way to
  // instantiate this class is via the three factory methods provided.
  ProcessingBasedContainer(ProcessingType processing_type,
                           Vector<EchoCancellationMode> echo_cancellation_modes,
                           BoolSet auto_gain_control_set,
                           BoolSet noise_suppression_set,
                           BoolSet voice_isolation_set,
                           IntRangeSet sample_size_range,
                           Vector<int> channels_set,
                           IntRangeSet sample_rate_range,
                           std::optional<SourceInfo> source_info,
                           AudioCaptureApi api,
                           media::AudioParameters device_parameters,
                           bool is_reconfiguration_allowed)
      : processing_type_(processing_type),
        sample_size_container_(sample_size_range),
        channels_container_(std::move(channels_set)),
        sample_rate_container_(sample_rate_range),
        latency_container_(
            GetAllowedLatency(processing_type, device_parameters)) {
    // If the device parameters indicate that system echo cancellation is
    // available, add support for it to `echo_cancellation_modes`.
    if (EchoCanceller::IsPlatformAecAvailable(device_parameters.effects())) {
      if (!base::Contains(echo_cancellation_modes,
                          EchoCancellationMode::kBrowserDecides)) {
        echo_cancellation_modes.push_back(
            EchoCancellationMode::kBrowserDecides);
      }
      if (ShouldSupportExtendedEchoCancellationModes(api) &&
          !base::Contains(echo_cancellation_modes,
                          EchoCancellationMode::kAll)) {
        echo_cancellation_modes.push_back(EchoCancellationMode::kAll);
      }
    }
    echo_cancellation_container_ = EchoCancellationContainer(
        std::move(echo_cancellation_modes), source_info, api, device_parameters,
        is_reconfiguration_allowed);

    auto_gain_control_container_ =
        AutoGainControlContainer(auto_gain_control_set);

    voice_isolation_container_ = VoiceIsolationContainer(voice_isolation_set);

    noise_suppression_container_ = BooleanContainer(noise_suppression_set);

    // Allow the full set of supported values when the device is not open or
    // when the candidate settings would open the device using an unprocessed
    // source.
    if (!source_info || (is_reconfiguration_allowed &&
                         processing_type_ == ProcessingType::kUnprocessed)) {
      return;
    }

    // If the device is already opened, restrict supported values for
    // non-reconfigurable settings to what is already configured. The rationale
    // for this is that opening multiple instances of the APM is costly.
    // TODO(crbug.com/1147928): Consider removing this restriction.
    auto_gain_control_container_ = AutoGainControlContainer(
        BoolSet({source_info->properties().auto_gain_control}));

    noise_suppression_container_ = BooleanContainer(
        BoolSet({source_info->properties().noise_suppression}));

    channels_container_ = IntegerDiscreteContainer({source_info->channels()});
    sample_rate_container_ = IntegerRangeContainer(
        IntRangeSet::FromValue(source_info->sample_rate()));
    latency_container_ =
        DoubleRangeContainer(DoubleRangeSet::FromValue(source_info->latency()));
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
        auto [min_latency, max_latency] =
            GetMinMaxLatenciesForAudioParameters(device_parameters);
        return DoubleRangeSet(min_latency, max_latency);
    }
  }

  // The priority of each processing-based container depends on the default
  // value assigned to the audio processing properties. When the value is true
  // the preference gives higher priority to the WebRTC processing.
  // On the contrary, if the value is false the preference is flipped towards
  // the option without processing.
  int GetProcessingPriority(
      const BooleanOrStringConstraint& ec_constraint) const {
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
  EchoCancellationContainer echo_cancellation_container_;
  AutoGainControlContainer auto_gain_control_container_;
  BooleanContainer noise_suppression_container_;
  VoiceIsolationContainer voice_isolation_container_;
  IntegerRangeContainer sample_size_container_;
  IntegerDiscreteContainer channels_container_;
  IntegerRangeContainer sample_rate_container_;
  DoubleRangeContainer latency_container_;
};

// Container for the constrainable properties of a single audio device.
class DeviceContainer {
 public:
  DeviceContainer(const AudioDeviceCaptureCapability& capability,
                  mojom::blink::MediaStreamType stream_type,
                  AudioCaptureApi api,
                  bool is_reconfiguration_allowed)
      : device_parameters_(capability.Parameters()) {
    if (!capability.DeviceID().empty()) {
      device_id_container_ =
          StringContainer(StringSet({capability.DeviceID().Utf8()}));
    }

    if (!capability.GroupID().empty()) {
      group_id_container_ =
          StringContainer(StringSet({capability.GroupID().Utf8()}));
    }

    // If the device is in use, a source will be provided and all containers
    // must be initialized such that their only supported values correspond to
    // the source settings. Otherwise, the containers are initialized to contain
    // all possible values.
    std::optional<SourceInfo> source_info =
        SourceInfo::FromSource(capability.source());

    // Three variations of the processing-based container. Each variant is
    // associated to a different type of audio processing configuration, namely
    // unprocessed, processed by WebRTC, or processed by other means.
    processing_based_containers_.push_back(
        ProcessingBasedContainer::CreateUnprocessedContainer(
            source_info, api, device_parameters_, is_reconfiguration_allowed));
    processing_based_containers_.push_back(
        ProcessingBasedContainer::CreateNoApmProcessedContainer(
            source_info, api, device_parameters_, is_reconfiguration_allowed));
    processing_based_containers_.push_back(
        ProcessingBasedContainer::CreateApmProcessedContainer(
            source_info, api, device_parameters_, is_reconfiguration_allowed));
    DCHECK_EQ(processing_based_containers_.size(), 3u);

    if (!source_info) {
      return;
    }

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

    for (const auto& info : kBooleanPropertyContainerInfoMap) {
      failed_constraint_name =
          boolean_containers_[info.index].ApplyConstraintSet(
              constraint_set.*(info.constraint_member));
      if (failed_constraint_name)
        return failed_constraint_name;
    }

    // For each processing based container, apply the constraints and only fail
    // if all of them failed.
    auto to_remove = std::ranges::remove_if(
        processing_based_containers_,
        [&constraint_set, &failed_constraint_name](auto& t) {
          DCHECK(!t.IsEmpty());
          failed_constraint_name = t.ApplyConstraintSet(constraint_set);
          return !!failed_constraint_name;
        });
    processing_based_containers_.erase(to_remove.begin(), to_remove.end());

    if (processing_based_containers_.empty()) {
      DCHECK_NE(failed_constraint_name, nullptr);
      return failed_constraint_name;
    }

    return nullptr;
  }

  std::tuple<Score, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_destkop_source,
      std::string default_device_id) const {
    DCHECK(!IsEmpty());
    Score score(0.0);

    auto [sub_score, device_id] = device_id_container_.SelectSettingsAndScore(
        constraint_set.device_id, default_device_id);
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
    std::optional<int> best_requested_buffer_size;
    int best_num_channels = 1;
    for (const auto& container : processing_based_containers_) {
      if (container.IsEmpty())
        continue;

      auto [container_score, container_properties, requested_buffer_size,
            num_channels] =
          container.SelectSettingsAndScore(constraint_set, device_parameters_);
      if (container_score > best_score) {
        best_score = container_score;
        best_properties = container_properties;
        best_container = &container;
        best_requested_buffer_size = requested_buffer_size;
        best_num_channels = num_channels;
      }
    }

    DCHECK_NE(best_container, nullptr);
    score += best_score;

    // The score at this point can be considered complete only when the settings
    // are compared against the default device id, which is used as arbitrator
    // in case multiple candidates are available.
    return std::make_tuple(
        score, AudioCaptureSettings(
                   device_id, best_requested_buffer_size, disable_local_echo,
                   render_to_associated_sink, best_container->processing_type(),
                   best_properties, best_num_channels));
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
  // DeviceContainer::boolean_containers_ and MediaTrackConstraintSetPlatform.
  struct BooleanPropertyContainerInfo {
    BooleanContainerId index;
    BooleanConstraint ConstraintSet::*constraint_member;
  };

  static constexpr BooleanPropertyContainerInfo
      kBooleanPropertyContainerInfoMap[] = {
          {kDisableLocalEcho, &ConstraintSet::disable_local_echo},
          {kRenderToAssociatedSink, &ConstraintSet::render_to_associated_sink}};

  media::AudioParameters device_parameters_;
  StringContainer device_id_container_;
  StringContainer group_id_container_;
  std::array<BooleanContainer, kNumBooleanContainerIds> boolean_containers_;
  Vector<ProcessingBasedContainer> processing_based_containers_;
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
                      mojom::blink::MediaStreamType stream_type,
                      std::string& media_stream_source,
                      std::string& default_device_id,
                      bool is_reconfiguration_allowed)
      : default_device_id_(default_device_id) {
    AudioCaptureApi api = GetAudioCaptureApi(stream_type, media_stream_source);
    for (const auto& capability : capabilities) {
      devices_.emplace_back(capability, stream_type, api,
                            is_reconfiguration_allowed);
      DCHECK(!devices_.back().IsEmpty());
    }
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* latest_failed_constraint_name = nullptr;
    auto to_remove = std::ranges::remove_if(
        devices_, [&constraint_set, &latest_failed_constraint_name](auto& t) {
          DCHECK(!t.IsEmpty());
          const auto* failed_constraint_name =
              t.ApplyConstraintSet(constraint_set);
          if (failed_constraint_name) {
            latest_failed_constraint_name = failed_constraint_name;
          }
          return !!failed_constraint_name;
        });
    devices_.erase(to_remove.begin(), to_remove.end());
    return IsEmpty() ? latest_failed_constraint_name : nullptr;
  }

  std::tuple<Score, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_desktop_source) const {
    DCHECK(!IsEmpty());
    // Make a copy of the settings initially provided, to track the default
    // settings.
    AudioCaptureSettings best_settings;
    Score best_score(-1.0);
    for (const auto& candidate : devices_) {
      auto [score, settings] = candidate.SelectSettingsAndScore(
          constraint_set, is_desktop_source, default_device_id_);

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
  Vector<DeviceContainer> devices_;
};

std::string GetMediaStreamSource(const MediaConstraints& constraints) {
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

V8UnionBooleanOrString* EchoCancellationModeToBooleanOrString(
    EchoCancellationMode mode) {
  switch (mode) {
    case EchoCancellationMode::kDisabled:
      return MakeGarbageCollected<V8UnionBooleanOrString>(false);
    case EchoCancellationMode::kBrowserDecides:
      return MakeGarbageCollected<V8UnionBooleanOrString>(true);
    case EchoCancellationMode::kAll:
      return MakeGarbageCollected<V8UnionBooleanOrString>(
          String(kEchoCancellationModeAll));
    case EchoCancellationMode::kRemoteOnly:
      return MakeGarbageCollected<V8UnionBooleanOrString>(
          String(kEchoCancellationModeRemoteOnly));
  }
}

Vector<EchoCancellationMode> GetSupportedEchoCancellationModes(
    int platform_effects,
    mojom::blink::MediaStreamType type) {
  Vector<EchoCancellationMode> result = {EchoCancellationMode::kBrowserDecides,
                                         EchoCancellationMode::kDisabled};
  if (RuntimeEnabledFeatures::GetUserMediaEchoCancellationModesEnabled() &&
      type == mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    result.push_back(EchoCancellationMode::kRemoteOnly);
#endif
    if (EchoCanceller::IsSystemWideAecAvailable(platform_effects)) {
      result.push_back(EchoCancellationMode::kAll);
    }
  }
  return result;
}

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
  DCHECK(!device_id_.empty());
}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    const AudioDeviceCaptureCapability&) = default;

AudioDeviceCaptureCapability& AudioDeviceCaptureCapability::operator=(
    const AudioDeviceCaptureCapability&) = default;

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
    const MediaConstraints& constraints,
    mojom::blink::MediaStreamType stream_type,
    bool is_reconfiguration_allowed) {
  if (capabilities.empty())
    return AudioCaptureSettings();

  std::string media_stream_source = GetMediaStreamSource(constraints);
  std::string default_device_id;
  AudioCaptureApi api = GetAudioCaptureApi(stream_type, media_stream_source);
  if ((api != AudioCaptureApi::kExtensionScreenShare) &&
      !capabilities.empty()) {
    default_device_id = capabilities.begin()->DeviceID().Utf8();
  }

  CandidatesContainer candidates(capabilities, stream_type, media_stream_source,
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
      media_stream_source == blink::kMediaStreamSourceDesktop);

  return settings;
}

AudioCaptureSettings SelectSettingsAudioCapture(
    blink::MediaStreamAudioSource* source,
    const MediaConstraints& constraints) {
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

  return SelectSettingsAudioCapture(capabilities, constraints,
                                    source->device().type,
                                    /*is_reconfiguration_allowed=*/false);
}

MODULES_EXPORT base::expected<Vector<blink::AudioCaptureSettings>, std::string>
SelectEligibleSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints,
    mojom::blink::MediaStreamType stream_type,
    bool is_reconfiguration_allowed) {
  Vector<AudioCaptureSettings> settings;
  std::string failed_constraint_name;
  for (const auto& device : capabilities) {
    const auto device_settings = SelectSettingsAudioCapture(
        {device}, constraints, stream_type, is_reconfiguration_allowed);
    if (device_settings.HasValue()) {
      settings.push_back(device_settings);
    } else {
      failed_constraint_name = device_settings.failed_constraint_name();
    }
  }

  if (settings.empty()) {
    return base::unexpected(failed_constraint_name);
  }
  return settings;
}

std::tuple<int, int> GetMinMaxBufferSizesForAudioParameters(
    const media::AudioParameters& parameters) {
  const int default_buffer_size = parameters.frames_per_buffer();
  DCHECK_GT(default_buffer_size, 0);

  const std::optional<media::AudioParameters::HardwareCapabilities>
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
  auto [min_buffer_size, max_buffer_size] =
      GetMinMaxBufferSizesForAudioParameters(parameters);

  // Doing the microseconds conversion to match what is done in
  // AudioParameters::GetBufferDuration() so that values reported to the user
  // are truncated consistently to the microseconds decimal place.
  return std::make_tuple(
      base::Microseconds(
          static_cast<int64_t>(min_buffer_size *
                               base::Time::kMicrosecondsPerSecond /
                               static_cast<float>(parameters.sample_rate())))
          .InSecondsF(),
      base::Microseconds(
          static_cast<int64_t>(max_buffer_size *
                               base::Time::kMicrosecondsPerSecond /
                               static_cast<float>(parameters.sample_rate())))
          .InSecondsF());
}

}  // namespace blink
