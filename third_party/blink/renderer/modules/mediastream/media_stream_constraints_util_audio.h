// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

// ============================================================================
// MediaStream Audio Constraints Resolution (W3C SelectSettings)
// ============================================================================
// Implements the W3C Media Capture and Streams `SelectSettings` algorithm for
// audio tracks (https://w3c.github.io/mediacapture-main/#dfn-selectsettings).
//
// The entry point `SelectSettingsAudioCapture()` evaluates JavaScript
// `MediaTrackConstraints` against available `AudioDeviceCaptureCapabilities`
// to select concrete `AudioCaptureSettings`:
//   - Target audio device (`deviceId`, `groupId`)
//   - Audio capture pipeline (`kUnprocessed`, `kNoApmProcessed`,
//     `kApmProcessed`)
//   - Audio processing properties (AEC mode, AGC, NS, Voice Isolation)
//   - Audio parameters (sample rate, channel count, buffer size / latency)
//
// Resolution Lifecycle:
// 1. Phase 1 (Filtering / Pruning):
//    - Mandatory Basic Constraints: Prunes all candidate devices and processing
//      pipelines that cannot satisfy the `Basic` constraint set. If all
//      candidates are eliminated, resolution fails with OverconstrainedError.
//    - Optional Advanced Constraints: Evaluates each `Advanced` constraint set
//      sequentially. If an advanced set would eliminate all remaining
//      candidates, it is discarded (best-effort matching).
// 2. Phase 2 (Ranking / Selection):
//    - Ranks surviving candidates using a composite lexicographical score:
//      Fitness distance to ideal constraints -> Default device preference ->
//      Echo cancellation mode preference -> Audio processing priority.
//
// Multi-Track Source Sharing & Sibling Constraints:
// - Tracks requiring WebRTC APM processing (kApmProcessed) share a single
//   underlying ProcessedLocalAudioSource for a given physical device to avoid
//   duplicate APM instances. An unprocessed track (kUnprocessed) can open a
//   separate LocalMediaStreamAudioSource alongside the processed one.
// - When resolving constraints for a new track attaching to an existing source,
//   the solver inspects all active sibling tracks sharing that source. If an
//   existing sibling has a mandatory exact constraint (e.g.
//   voiceIsolation: {exact: true}), candidate settings contradicting that
//   constraint are pruned.
// - Only exact constraints from the mandatory Basic set are recorded on
//   tracks and enforced against siblings (Advanced sets are optional and
//   ignored if unsatisfied).
//
// Dynamic Reconfiguration Asymmetry (`is_full_reconfiguration_allowed`):
// - Initial `getUserMedia()` sets `is_full_reconfiguration_allowed = true`,
//   allowing the engine to consider all processing pipelines (`kUnprocessed`,
//   `kNoApmProcessed`, `kApmProcessed`).
// - `MediaStreamTrack.applyConstraints()` sets
//   `is_full_reconfiguration_allowed = false` because Chromium does not
//   currently support dynamically reconfiguring a live audio source pipeline
//   in-place (crbug.com/796964). The solver therefore confines candidates
//   strictly to the active source's existing processing type.

namespace blink {
class MediaConstraints;
class MediaStreamAudioSource;
class MediaStreamAudioTrack;
class V8UnionBooleanOrString;
}  // namespace blink

namespace blink {

V8UnionBooleanOrString* EchoCancellationModeToBooleanOrString(
    EchoCancellationMode);
Vector<EchoCancellationMode> GetSupportedEchoCancellationModes(
    int platform_effects,
    mojom::blink::MediaStreamType);

// Returns the set of supported values for the voiceIsolation constraint on a
// specific device, given its hardware platform effects.
// If voice isolation is supported by the platform and the device, returns
// {true, false}. Otherwise, returns {false} (only disabled state is supported).
Vector<bool> GetSupportedVoiceIsolationValues(int platform_effects);

// Returns true if voice isolation is supported by the platform (either
// system-wide or browser-side).
MODULES_EXPORT bool IsVoiceIsolationSupported();

// This class represents the capability of an audio-capture device.
// It may represent three different things:
// 1. An audio-capture device that is currently in use.
// 2. An audio-capture device that is currently not in use, but whose ID and
//    parameters are known (suitable for device capture, where device IDs are
//    always known).
// 3. A "device" whose ID is not known (suitable for content capture, where
//    it is not possible to have a list of known valid device IDs).
// In cases (1) and (2), the known device ID introduces a restriction on the
// acceptable values for the deviceId constraint, while in case (3) no such
// restriction is imposed and any requested deviceID value will be acceptable
// while processing constraints.
class MODULES_EXPORT AudioDeviceCaptureCapability {
 public:
  // This creates an AudioDeviceCaptureCapability that admits all possible
  // device names and settings. This is intended to be used as the single
  // capability for getUserMedia() with content capture, where the set of valid
  // device IDs is infinite.
  AudioDeviceCaptureCapability();

  // This creates an AudioDeviceCaptureCapability where the device ID is limited
  // to |device_id|, the group ID is limited to |group_id| and other settings
  // are limited by the given |parameters|. |device_id| must not be empty.
  // Intended to be used by getUserMedia() with device capture for devices that
  // are not currently in use.
  AudioDeviceCaptureCapability(String device_id,
                               String group_id,
                               const media::AudioParameters& parameters);

  // This creates an AudioDeviceCaptureCapability where the device ID and other
  // settings are restricted to the current settings of |source|. Intended to be
  // used by applyConstraints() for both device and content capture, and by
  // getUserMedia() with device capture for devices that are currently in use.
  explicit AudioDeviceCaptureCapability(blink::MediaStreamAudioSource* source);

  AudioDeviceCaptureCapability(const AudioDeviceCaptureCapability&);
  AudioDeviceCaptureCapability& operator=(const AudioDeviceCaptureCapability&);

  // If this capability represents a device currently in use, this method
  // returns a pointer to the MediaStreamAudioSource object associated with the
  // device. Otherwise, it returns null.
  blink::MediaStreamAudioSource* source() const { return source_; }

  // Returns the ID of the device associated with this capability. If empty,
  // it means that this capability is not associated with a known device and
  // no restrictions are imposed on the deviceId or other constraints while
  // processing constraints.
  String DeviceID() const;

  // Returns the group ID of the device associated with this capability.
  String GroupID() const;

  // Returns the audio parameters for the device associated with this
  // capability. If DeviceID() returns an empty string, these parameters contain
  // default values that work well for content capture.
  const media::AudioParameters& Parameters() const;
  std::optional<bool> GetVoiceIsolationExactConstraint(
      const MediaStreamAudioTrack* current_track = nullptr) const;

 private:
  raw_ptr<blink::MediaStreamAudioSource> source_ = nullptr;
  String device_id_;
  String group_id_;
  media::AudioParameters parameters_;
};

using AudioDeviceCaptureCapabilities = Vector<AudioDeviceCaptureCapability>;

// This function implements the SelectSettings algorithm for audio tracks as
// described in https://w3c.github.io/mediacapture-main/#dfn-selectsettings
// The algorithm starts with a set containing all possible candidate settings
// based on system/hardware capabilities (passed via the |capabilities|
// parameter) and supported values for properties not involved in device
// selection. Candidates that do not support the basic constraint set from
// |constraints| are removed. If the set of candidates is empty after this step,
// the function returns an AudioCaptureSettings object without value and whose
// failed_constraint_name() method returns the name of one of the (possibly
// many) constraints that could not be satisfied or an empty string if the set
// of candidates was initially empty (e.g., if there are no devices in the
// system). After the basic constraint set is applied, advanced constraint sets
// are applied. If no candidates can satisfy an advanced set, the advanced set
// is ignored, otherwise the candidates that cannot satisfy the advanced set are
// removed.
// Once all constraint sets are applied, the result is selected from the
// remaining candidates by giving preference to candidates closest to the ideal
// values specified in the basic constraint set, or using default
// implementation-specific values.
// The result includes the following properties:
//  * Device. A device can be chosen using the device_id constraint.
//    For device capture, the validity of device IDs is checked by
//    SelectSettings since the list of allowed device IDs is known in advance.
//    For content capture, all device IDs are considered valid by
//    SelectSettings. Actual validation is performed by the getUserMedia
//    implementation.
//  * Audio features: the disable_local_echo and render_to_associated_sink
//    constraints can be used to enable the corresponding audio feature. If not
//    specified, their default value is false, except for disable_local_echo,
//    whose default value is false only for desktop capture.
//  * Audio processing. The remaining constraints are used to control audio
//    processing. This is how audio-processing properties are set for device
//    capture(see the blink::AudioProcessingProperties struct) :
//      - echo_cancellation_type: mapped from the experimental constraint with
//        the same name. "System" is selected only if the device supports it.
//        If constraint is not specified, "system" is selected if supported,
//        with exception for experimental system echo cancellation.
//    The remaining audio-processing properties are directly mapped from the
//    final value of the corresponding constraints. If no value is explicitly
//    specified, the default value is the same as the final value of the
//    echo_cancellation constraint.  If the echo_cancellation constraint is
//    not explicitly specified, the default value is implementation defined
//    (see blink::AudioProcessingProperties).
//    For content capture the rules are the same, but all properties are false
//    by default, regardless of the value of the echo_cancellation constraint.
//    Note that it is important to distinguish between audio properties and
//    constraints. Constraints are an input to SelectSettings, while properties
//    are part of the output. The value for most boolean properties comes
//    directly from a corresponding boolean constraint, but this is not true for
//    all constraints and properties. For example, the echo_cancellation is not
//    directly mapped to any property, but, together with hardware
//    characteristics, influence the selection of echo cancellation type.
//    Moreover, the echo_cancellation constraint influences most other
//    audio-processing properties for which no explicit value is provided in
//    their corresponding constraints.
// |is_full_reconfiguration_allowed| indicates whether it is possible to
// reconfigure settings on an open audio track.
// TODO(crbug.com/796964): remove |is_full_reconfiguration_allowed| when both
// getUserMedia and applyConstraints code paths allow for reconfiguration.
MODULES_EXPORT blink::AudioCaptureSettings SelectSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints,
    mojom::blink::MediaStreamType stream_type,
    bool is_full_reconfiguration_allowed,
    const MediaStreamAudioTrack* current_track = nullptr);

// This variant of SelectSettings takes an existing MediaStreamAudioSource
// as input in order to determine settings that are compatible with it.
// This is intended to be used by applyConstraints().
// The current implementation rejects constraints that would result in settings
// different from those of |source| because it is currently not possible to
// reconfigure audio tracks or sources.
// TODO(guidou): Allow reconfiguring audio tracks. https://crbug.com/796964
MODULES_EXPORT blink::AudioCaptureSettings SelectSettingsAudioCapture(
    blink::MediaStreamAudioSource* source,
    const MediaConstraints& constraints,
    const MediaStreamAudioTrack* current_track = nullptr);

// Selects settings for each eligible device in `capabilities` in isolation and
// returns them as a vector. If none of the devices are eligible, then the name
// of one of the failed constraints is returned.
MODULES_EXPORT base::expected<Vector<blink::AudioCaptureSettings>, std::string>
SelectEligibleSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints,
    mojom::blink::MediaStreamType stream_type,
    bool is_full_reconfiguration_allowed);

// Return a tuple with <min,max> representing the min and max buffer sizes or
// latencies that can be provided by the given AudioParameters. The min and max
// are guaranteed to be > 0 and with max >= min.
MODULES_EXPORT std::tuple<int, int> GetMinMaxBufferSizesForAudioParameters(
    const media::AudioParameters& parameters);
MODULES_EXPORT std::tuple<double, double> GetMinMaxLatenciesForAudioParameters(
    const media::AudioParameters& parameters);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_AUDIO_H_
