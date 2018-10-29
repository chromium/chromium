// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "content/common/media/media_stream_controls.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_constraints_util_sets.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;

namespace {

template <class T>
using DiscreteSet = media_constraints::DiscreteSet<T>;

enum BoolConstraint {
  // Constraints not related to audio processing.
  HOTWORD_ENABLED,
  DISABLE_LOCAL_ECHO,
  RENDER_TO_ASSOCIATED_SINK,

  // Constraints that enable/disable audio processing.
  ECHO_CANCELLATION,
  GOOG_ECHO_CANCELLATION,

  // Constraints that control audio-processing behavior.
  GOOG_AUDIO_MIRRORING,
  GOOG_AUTO_GAIN_CONTROL,
  GOOG_EXPERIMENTAL_ECHO_CANCELLATION,
  GOOG_TYPING_NOISE_DETECTION,
  GOOG_NOISE_SUPPRESSION,
  GOOG_EXPERIMENTAL_NOISE_SUPPRESSION,
  GOOG_HIGHPASS_FILTER,
  GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL,
  NUM_BOOL_CONSTRAINTS
};

// This struct groups related fields or entries from AudioProcessingProperties,
// SingleDeviceCandidateSet::bool_sets_ and blink::WebMediaTrackConstraintSet.
struct AudioPropertyConstraintPair {
  bool AudioProcessingProperties::* audio_property;
  BoolConstraint bool_set_index;
};

// Boolean audio properties that are mapped directly to a boolean constraint
// and which are subject to the same processing.
const AudioPropertyConstraintPair kAudioPropertyConstraintMap[] = {
    {&AudioProcessingProperties::goog_auto_gain_control,
     GOOG_AUTO_GAIN_CONTROL},
    {&AudioProcessingProperties::goog_experimental_echo_cancellation,
     GOOG_EXPERIMENTAL_ECHO_CANCELLATION},
    {&AudioProcessingProperties::goog_typing_noise_detection,
     GOOG_TYPING_NOISE_DETECTION},
    {&AudioProcessingProperties::goog_noise_suppression,
     GOOG_NOISE_SUPPRESSION},
    {&AudioProcessingProperties::goog_experimental_noise_suppression,
     GOOG_EXPERIMENTAL_NOISE_SUPPRESSION},
    {&AudioProcessingProperties::goog_highpass_filter, GOOG_HIGHPASS_FILTER},
    {&AudioProcessingProperties::goog_experimental_auto_gain_control,
     GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL}};

// Selects the best value from the nonempty |set|, subject to |constraint|. The
// first selection criteria is equality to |constraint.Ideal()|, followed by
// equality to |default_value|. There is always a single best value.
bool SelectBool(const DiscreteSet<bool>& set,
                const blink::BooleanConstraint& constraint,
                bool default_value) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal()))
    return constraint.Ideal();

  if (set.is_universal())
    return default_value;

  DCHECK_EQ(set.elements().size(), 1U);
  return set.FirstElement();
}

// Selects the best value from the nonempty |set|, subject to |constraint|. The
// only decision criteria is equality to |constraint.Ideal()|. If there is no
// single best value in |set|, returns nullopt.
base::Optional<bool> SelectOptionalBool(
    const DiscreteSet<bool>& set,
    const blink::BooleanConstraint& constraint) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal()))
    return constraint.Ideal();

  if (set.is_universal())
    return base::Optional<bool>();

  DCHECK_EQ(set.elements().size(), 1U);
  return set.FirstElement();
}

// Selects the best value from the nonempty |set|, subject to |constraint|. The
// first selection criteria is inclusion in |constraint.Ideal()|, followed by
// equality to |default_value|. There is always a single best value.
std::string SelectString(const DiscreteSet<std::string>& set,
                         const blink::StringConstraint& constraint,
                         const std::string& default_value) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal()) {
    for (const blink::WebString& ideal_candidate : constraint.Ideal()) {
      std::string candidate = ideal_candidate.Utf8();
      if (set.Contains(candidate))
        return candidate;
    }
  }

  if (set.Contains(default_value))
    return default_value;

  return set.FirstElement();
}

bool SelectUseEchoCancellation(base::Optional<bool> echo_cancellation,
                               base::Optional<bool> goog_echo_cancellation,
                               bool is_device_capture) {
  DCHECK(echo_cancellation && goog_echo_cancellation
             ? *echo_cancellation == *goog_echo_cancellation
             : true);
  if (echo_cancellation)
    return *echo_cancellation;
  if (goog_echo_cancellation)
    return *goog_echo_cancellation;

  // Echo cancellation is enabled by default for device capture and disabled by
  // default for content capture.
  return is_device_capture;
}

std::vector<std::string> GetEchoCancellationTypesFromParameters(
    const media::AudioParameters& audio_parameters) {
  if (audio_parameters.effects() &
      (media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER |
       media::AudioParameters::ECHO_CANCELLER)) {
    // If the system/hardware supports echo cancellation, return all echo
    // cancellers.
    return {blink::kEchoCancellationTypeBrowser,
            blink::kEchoCancellationTypeAec3,
            blink::kEchoCancellationTypeSystem};
  }

  // The browser and AEC3 echo cancellers are always available.
  return {blink::kEchoCancellationTypeBrowser,
          blink::kEchoCancellationTypeAec3};
}

// This class represents all the candidates settings for a single audio device.
class SingleDeviceCandidateSet {
 public:
  explicit SingleDeviceCandidateSet(
      const AudioDeviceCaptureCapability& capability)
      : parameters_(capability.Parameters()) {
    // If empty, all values for the deviceId constraint are allowed and
    // |device_id_set_| is the universal set. Otherwise, limit |device_id_set_|
    // to the known device ID.
    if (!capability.DeviceID().empty())
      device_id_set_ = DiscreteSet<std::string>({capability.DeviceID()});

    if (!capability.GroupID().empty())
      group_id_set_ = DiscreteSet<std::string>({capability.GroupID()});

    MediaStreamAudioSource* source = capability.source();

    // Set up echo cancellation types. Depending on if we have a source or not
    // it's set up differently.
    if (!source) {
      echo_cancellation_type_set_ = DiscreteSet<std::string>(
          GetEchoCancellationTypesFromParameters(parameters_));
      return;
    }

    // Properties not related to audio processing.
    bool_sets_[HOTWORD_ENABLED] =
        DiscreteSet<bool>({source->hotword_enabled()});
    bool_sets_[DISABLE_LOCAL_ECHO] =
        DiscreteSet<bool>({source->disable_local_echo()});
    bool_sets_[RENDER_TO_ASSOCIATED_SINK] =
        DiscreteSet<bool>({source->RenderToAssociatedSinkEnabled()});

    // Properties related with audio processing.
    AudioProcessingProperties properties;
    ProcessedLocalAudioSource* processed_source =
        ProcessedLocalAudioSource::From(source);
    if (processed_source)
      properties = processed_source->audio_processing_properties();
    else
      properties.DisableDefaultProperties();

    const bool system_echo_cancellation_available =
        (properties.echo_cancellation_type ==
             EchoCancellationType::kEchoCancellationSystem ||
         !processed_source) &&
        parameters_.effects() & media::AudioParameters::ECHO_CANCELLER;

    const bool experimental_system_cancellation_available =
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationSystem &&
        parameters_.effects() &
            media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;

    const bool system_echo_cancellation_enabled =
        system_echo_cancellation_available ||
        experimental_system_cancellation_available;

    const bool echo_cancellation_enabled =
        properties.EchoCancellationIsWebRtcProvided() ||
        system_echo_cancellation_enabled;

    bool_sets_[ECHO_CANCELLATION] =
        DiscreteSet<bool>({echo_cancellation_enabled});
    bool_sets_[GOOG_ECHO_CANCELLATION] = bool_sets_[ECHO_CANCELLATION];

    if (properties.echo_cancellation_type ==
        EchoCancellationType::kEchoCancellationAec2) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeBrowser});
    } else if (properties.echo_cancellation_type ==
               EchoCancellationType::kEchoCancellationAec3) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeAec3});
    } else if (system_echo_cancellation_enabled) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeSystem});
    }

    bool_sets_[GOOG_AUDIO_MIRRORING] =
        DiscreteSet<bool>({properties.goog_audio_mirroring});

    for (auto& entry : kAudioPropertyConstraintMap) {
      bool_sets_[entry.bool_set_index] =
          DiscreteSet<bool>({properties.*entry.audio_property});
    }

#if DCHECK_IS_ON()
    for (const auto& bool_set : bool_sets_) {
      DCHECK(!bool_set.is_universal());
      DCHECK(!bool_set.IsEmpty());
    }
#endif
  }

  // Accessors
  const char* failed_constraint_name() const { return failed_constraint_name_; }
  const DiscreteSet<std::string>& device_id_set() const {
    return device_id_set_;
  }

  bool IsEmpty() const { return failed_constraint_name_ != nullptr; }

  void ApplyConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set) {
    device_id_set_ = device_id_set_.Intersection(
        media_constraints::StringSetFromConstraint(constraint_set.device_id));
    if (device_id_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.device_id.GetName();
      return;
    }

    group_id_set_ = group_id_set_.Intersection(
        media_constraints::StringSetFromConstraint(constraint_set.group_id));
    if (group_id_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.group_id.GetName();
      return;
    }

    for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
      bool_sets_[i] =
          bool_sets_[i].Intersection(media_constraints::BoolSetFromConstraint(
              constraint_set.*kBlinkBoolConstraintFields[i]));
      if (bool_sets_[i].IsEmpty()) {
        failed_constraint_name_ =
            (constraint_set.*kBlinkBoolConstraintFields[i]).GetName();
        return;
      }
    }

    // echoCancellation and googEchoCancellation constraints should not
    // contradict each other. Mark the set as empty if they do.
    DiscreteSet<bool> echo_cancellation_intersection =
        bool_sets_[ECHO_CANCELLATION].Intersection(
            bool_sets_[GOOG_ECHO_CANCELLATION]);
    if (echo_cancellation_intersection.IsEmpty()) {
      failed_constraint_name_ =
          blink::WebMediaTrackConstraintSet().echo_cancellation.GetName();
      return;
    }

    echo_cancellation_type_set_ = echo_cancellation_type_set_.Intersection(
        media_constraints::StringSetFromConstraint(
            constraint_set.echo_cancellation_type));
    if (echo_cancellation_type_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.echo_cancellation_type.GetName();
      return;
    }

    // If echo cancellation constraint is not true, the type set should not have
    // explicit elements.
    if (!bool_sets_[ECHO_CANCELLATION].Contains(true) &&
        constraint_set.echo_cancellation_type.HasExact()) {
      failed_constraint_name_ = constraint_set.echo_cancellation_type.GetName();
      return;
    }
  }

  // Fitness function to support device selection. Based on
  // https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
  double Fitness(
      const blink::WebMediaTrackConstraintSet& constraint_set) const {
    double fitness = 0.0;

    if (constraint_set.device_id.HasIdeal()) {
      for (const blink::WebString& ideal_value :
           constraint_set.device_id.Ideal()) {
        if (device_id_set_.Contains(ideal_value.Utf8())) {
          fitness += 1.0;
          break;
        }
      }
    }

    if (constraint_set.group_id.HasIdeal()) {
      for (const blink::WebString& ideal_value :
           constraint_set.group_id.Ideal()) {
        if (group_id_set_.Contains(ideal_value.Utf8())) {
          fitness += 1.0;
          break;
        }
      }
    }

    for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
      if ((constraint_set.*kBlinkBoolConstraintFields[i]).HasIdeal() &&
          bool_sets_[i].Contains(
              (constraint_set.*kBlinkBoolConstraintFields[i]).Ideal())) {
        fitness += 1.0;
      }
    }

    // If echo cancellation constraint is not set to true, the type shall be
    // ignored.
    if ((constraint_set.*kBlinkBoolConstraintFields[ECHO_CANCELLATION])
            .Matches(true) &&
        constraint_set.echo_cancellation_type.HasIdeal()) {
      for (const blink::WebString& ideal_value :
           constraint_set.echo_cancellation_type.Ideal()) {
        if (echo_cancellation_type_set_.Contains(ideal_value.Utf8())) {
          fitness += 1.0;
          break;
        }
      }
    }

    return fitness;
  }

  // Returns the settings supported by this SingleDeviceCandidateSet that best
  // satisfy the ideal values in |basic_constraint_set|.
  AudioCaptureSettings SelectBestSettings(
      const blink::WebMediaTrackConstraintSet& basic_constraint_set,
      const std::string& default_device_id,
      const std::string& media_stream_source,
      bool should_disable_hardware_noise_suppression) const {
    std::string device_id = SelectString(
        device_id_set_, basic_constraint_set.device_id, default_device_id);
    bool hotword_enabled =
        SelectBool(bool_sets_[HOTWORD_ENABLED],
                   basic_constraint_set.hotword_enabled, false);
    bool disable_local_echo_default =
        media_stream_source != kMediaStreamSourceDesktop;
    bool disable_local_echo = SelectBool(
        bool_sets_[DISABLE_LOCAL_ECHO], basic_constraint_set.disable_local_echo,
        disable_local_echo_default);
    bool render_to_associated_sink =
        SelectBool(bool_sets_[RENDER_TO_ASSOCIATED_SINK],
                   basic_constraint_set.render_to_associated_sink, false);

    bool is_device_capture = media_stream_source.empty();
    AudioProcessingProperties audio_processing_properties =
        SelectAudioProcessingProperties(
            basic_constraint_set, is_device_capture,
            should_disable_hardware_noise_suppression);

    return AudioCaptureSettings(std::move(device_id), hotword_enabled,
                                disable_local_echo, render_to_associated_sink,
                                audio_processing_properties);
  }

 private:
  EchoCancellationType SelectEchoCancellationType(
      const blink::StringConstraint& echo_cancellation_type,
      const media::AudioParameters& audio_parameters) const {
    // Try to use an ideal candidate, if supplied.
    base::Optional<std::string> selected_type;
    if (echo_cancellation_type.HasIdeal()) {
      for (const auto& ideal : echo_cancellation_type.Ideal()) {
        std::string candidate = ideal.Utf8();
        if (echo_cancellation_type_set_.Contains(candidate)) {
          selected_type = candidate;
          break;
        }
      }
    }

    // If no ideal, or none that worked, and the set contains only one value,
    // pick that.
    if (!selected_type) {
      if (!echo_cancellation_type_set_.is_universal() &&
          echo_cancellation_type_set_.elements().size() == 1) {
        selected_type = echo_cancellation_type_set_.FirstElement();
      }
    }

    // Return type based the selected type.
    if (selected_type == blink::kEchoCancellationTypeBrowser) {
      return EchoCancellationType::kEchoCancellationAec2;
    } else if (selected_type == blink::kEchoCancellationTypeAec3) {
      return EchoCancellationType::kEchoCancellationAec3;
    } else if (selected_type == blink::kEchoCancellationTypeSystem) {
      return EchoCancellationType::kEchoCancellationSystem;
    }

    // If no type has been selected, choose system if the device has the
    // ECHO_CANCELLER flag set. Never automatically enable an experimental
    // system echo canceller.
    if (audio_parameters.IsValid() &&
        audio_parameters.effects() & media::AudioParameters::ECHO_CANCELLER) {
      return EchoCancellationType::kEchoCancellationSystem;
    }

    // Finally, choose the browser provided AEC2 or AEC3 based on an optional
    // override setting for AEC3 or feature.
    // In unit tests not creating a message filter, |aec_dump_message_filter|
    // will be null. We can just ignore that. Other unit tests and browser tests
    // ensure that we do get the filter when we should.
    base::Optional<bool> override_aec3;
    scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter =
        AecDumpMessageFilter::Get();
    if (aec_dump_message_filter)
      override_aec3 = aec_dump_message_filter->GetOverrideAec3();
    const bool use_aec3 = override_aec3.value_or(
        base::FeatureList::IsEnabled(features::kWebRtcUseEchoCanceller3));

    return use_aec3 ? EchoCancellationType::kEchoCancellationAec3
                    : EchoCancellationType::kEchoCancellationAec2;
  }

  // Returns the audio-processing properties supported by this
  // SingleDeviceCandidateSet that best satisfy the ideal values in
  // |basic_constraint_set|.
  AudioProcessingProperties SelectAudioProcessingProperties(
      const blink::WebMediaTrackConstraintSet& basic_constraint_set,
      bool is_device_capture,
      bool should_disable_hardware_noise_suppression) const {
    DCHECK(!IsEmpty());
    base::Optional<bool> echo_cancellation = SelectOptionalBool(
        bool_sets_[ECHO_CANCELLATION], basic_constraint_set.echo_cancellation);
    // Audio-processing properties are disabled by default for content capture,
    // or if the |echo_cancellation| constraint is false.
    bool default_audio_processing_value = true;
    if (!is_device_capture || (echo_cancellation && !*echo_cancellation))
      default_audio_processing_value = false;

    base::Optional<bool> goog_echo_cancellation =
        SelectOptionalBool(bool_sets_[GOOG_ECHO_CANCELLATION],
                           basic_constraint_set.goog_echo_cancellation);

    const bool use_echo_cancellation = SelectUseEchoCancellation(
        echo_cancellation, goog_echo_cancellation, is_device_capture);

    AudioProcessingProperties properties;
    if (use_echo_cancellation) {
      properties.echo_cancellation_type = SelectEchoCancellationType(
          basic_constraint_set.echo_cancellation_type, parameters_);
    } else {
      properties.echo_cancellation_type =
          EchoCancellationType::kEchoCancellationDisabled;
    }

    properties.disable_hw_noise_suppression =
        should_disable_hardware_noise_suppression &&
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationSystem;

    properties.goog_audio_mirroring =
        SelectBool(bool_sets_[GOOG_AUDIO_MIRRORING],
                   basic_constraint_set.goog_audio_mirroring,
                   properties.goog_audio_mirroring);

    for (auto& entry : kAudioPropertyConstraintMap) {
      properties.*entry.audio_property = SelectBool(
          bool_sets_[entry.bool_set_index],
          basic_constraint_set.*
              kBlinkBoolConstraintFields[entry.bool_set_index],
          default_audio_processing_value && properties.*entry.audio_property);
    }

    return properties;
  }

  static constexpr blink::BooleanConstraint
      blink::WebMediaTrackConstraintSet::* const
          kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS] = {
              &blink::WebMediaTrackConstraintSet::hotword_enabled,
              &blink::WebMediaTrackConstraintSet::disable_local_echo,
              &blink::WebMediaTrackConstraintSet::render_to_associated_sink,
              &blink::WebMediaTrackConstraintSet::echo_cancellation,
              &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
              &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
              &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
              &blink::WebMediaTrackConstraintSet::
                  goog_experimental_echo_cancellation,
              &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
              &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
              &blink::WebMediaTrackConstraintSet::
                  goog_experimental_noise_suppression,
              &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
              &blink::WebMediaTrackConstraintSet::
                  goog_experimental_auto_gain_control};

  const char* failed_constraint_name_ = nullptr;
  DiscreteSet<std::string> device_id_set_;
  DiscreteSet<std::string> group_id_set_;
  std::array<DiscreteSet<bool>, NUM_BOOL_CONSTRAINTS> bool_sets_;
  DiscreteSet<std::string> echo_cancellation_type_set_;
  media::AudioParameters parameters_;
};

constexpr blink::BooleanConstraint blink::WebMediaTrackConstraintSet::* const
    SingleDeviceCandidateSet::kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS];

// This class represents a set of possible candidate settings.
// The SelectSettings algorithm starts with a set containing all possible
// candidates based on system/hardware capabilities and/or allowed values for
// supported properties. The set is then reduced progressively as the basic and
// advanced constraint sets are applied. In the end, if the set of candidates is
// empty, SelectSettings fails. If not, the ideal values (if any) or tie breaker
// rules are used to select the final settings based on the candidates that
// survived the application of the constraint sets. This class is implemented as
// a collection of more specific sets for the various supported properties. If
// any of the specific sets is empty, the whole AudioCaptureCandidates set is
// considered empty as well.
class AudioCaptureCandidates {
 public:
  AudioCaptureCandidates(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const AudioDeviceCaptureCapabilities& capabilities) {
    for (const auto& capability : capabilities)
      candidate_sets_.emplace_back(capability);

    ApplyConstraintSet(constraint_set);
  }

  const char* failed_constraint_name() const { return failed_constraint_name_; }
  bool IsEmpty() const { return failed_constraint_name_ != nullptr; }

  void ApplyConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set) {
    for (auto& candidate_set : candidate_sets_)
      candidate_set.ApplyConstraintSet(constraint_set);

    const char* failed_constraint_name = nullptr;
    for (auto it = candidate_sets_.begin(); it != candidate_sets_.end();) {
      if (it->IsEmpty()) {
        DCHECK(it->failed_constraint_name());
        failed_constraint_name = it->failed_constraint_name();
        it = candidate_sets_.erase(it);
      } else {
        ++it;
      }
    }

    if (candidate_sets_.empty())
      failed_constraint_name_ = failed_constraint_name;
  }

  // Returns the settings that best satisfy the ideal values in
  // |basic_constraint_set| subject to the limitations of this
  // AudioCaptureCandidates object.
  AudioCaptureSettings SelectBestSettings(
      const blink::WebMediaTrackConstraintSet& basic_constraint_set,
      const std::string& default_device_id,
      const std::string& media_stream_source,
      bool should_disable_hardware_noise_suppression) const {
    const SingleDeviceCandidateSet* device_candidate_set =
        SelectBestDevice(basic_constraint_set, default_device_id);
    DCHECK(!device_candidate_set->IsEmpty());
    return device_candidate_set->SelectBestSettings(
        basic_constraint_set, default_device_id, media_stream_source,
        should_disable_hardware_noise_suppression);
  }

 private:
  // Selects the best device based on the fitness function.
  // The returned pointer is valid as long as |candidate_sets_| is not mutated.
  const SingleDeviceCandidateSet* SelectBestDevice(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const std::string& default_device_id) const {
    DCHECK(!candidate_sets_.empty());

    auto best_candidate = candidate_sets_.end();
    std::vector<double> best_fitness({-1, -1});

    for (auto it = candidate_sets_.begin(); it != candidate_sets_.end(); ++it) {
      DCHECK(!it->IsEmpty());
      std::vector<double> fitness;
      fitness.push_back(it->Fitness(constraint_set));
      // Second selection criterion is being the default device.
      fitness.push_back(it->device_id_set().Contains(default_device_id) ? 1.0
                                                                        : 0.0);
      if (fitness > best_fitness) {
        best_fitness = fitness;
        best_candidate = it;
      }
    }

    return &(*best_candidate);
  }

  const char* failed_constraint_name_ = nullptr;
  std::vector<SingleDeviceCandidateSet> candidate_sets_;
};

}  // namespace

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability()
    : parameters_(media::AudioParameters::UnavailableDeviceParams()) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    MediaStreamAudioSource* source)
    : source_(source) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    std::string device_id,
    std::string group_id,
    const media::AudioParameters& parameters)
    : device_id_(std::move(device_id)),
      group_id_(std::move(group_id)),
      parameters_(parameters) {
  DCHECK(!device_id_.empty());
}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    const AudioDeviceCaptureCapability& other) = default;

const std::string& AudioDeviceCaptureCapability::DeviceID() const {
  return source_ ? source_->device().id : device_id_;
}

const std::string& AudioDeviceCaptureCapability::GroupID() const {
  return source_ && source_->device().group_id ? *source_->device().group_id
                                               : group_id_;
}

const media::AudioParameters& AudioDeviceCaptureCapability::Parameters() const {
  return source_ ? source_->device().input : parameters_;
}

AudioCaptureSettings SelectSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const blink::WebMediaConstraints& constraints,
    bool should_disable_hardware_noise_suppression) {
  std::string media_stream_source = GetMediaStreamSource(constraints);
  bool is_device_capture = media_stream_source.empty();
  if (capabilities.empty())
    return AudioCaptureSettings();

  AudioCaptureCandidates candidates(constraints.Basic(), capabilities);
  if (candidates.IsEmpty())
    return AudioCaptureSettings(candidates.failed_constraint_name());

  for (const auto& advanced_set : constraints.Advanced()) {
    AudioCaptureCandidates copy = candidates;
    candidates.ApplyConstraintSet(advanced_set);
    if (candidates.IsEmpty())
      candidates = std::move(copy);
  }
  DCHECK(!candidates.IsEmpty());

  std::string default_device_id;
  if (is_device_capture)
    default_device_id = capabilities.begin()->DeviceID();

  return candidates.SelectBestSettings(
      constraints.Basic(), default_device_id, media_stream_source,
      should_disable_hardware_noise_suppression);
}

AudioCaptureSettings CONTENT_EXPORT
SelectSettingsAudioCapture(MediaStreamAudioSource* source,
                           const blink::WebMediaConstraints& constraints) {
  DCHECK(source);
  if (source->device().type != MEDIA_DEVICE_AUDIO_CAPTURE &&
      source->device().type != MEDIA_GUM_TAB_AUDIO_CAPTURE &&
      source->device().type != MEDIA_GUM_DESKTOP_AUDIO_CAPTURE) {
    return AudioCaptureSettings();
  }

  std::string media_stream_source = GetMediaStreamSource(constraints);
  if (source->device().type == MEDIA_DEVICE_AUDIO_CAPTURE &&
      !media_stream_source.empty()) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }

  if (source->device().type == MEDIA_GUM_TAB_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != kMediaStreamSourceTab) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }
  if (source->device().type == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != kMediaStreamSourceSystem &&
      media_stream_source != kMediaStreamSourceDesktop) {
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

}  // namespace content
