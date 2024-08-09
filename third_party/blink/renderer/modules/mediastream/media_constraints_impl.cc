/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace media_constraints_impl {

namespace {

// A naked value is treated as an "ideal" value in the basic constraints,
// but as an exact value in "advanced" constraints.
// https://w3c.github.io/mediacapture-main/#constrainable-interface
enum class NakedValueDisposition { kTreatAsIdeal, kTreatAsExact };

// Old type/value form of constraint. Used in parsing old-style constraints.
struct NameValueStringConstraint {
  NameValueStringConstraint() = default;

  NameValueStringConstraint(String name, String value)
      : name_(name), value_(value) {}

  String name_;
  String value_;
};

// Legal constraint names.

// Legacy getUserMedia() constraints. Sadly still in use.
const char kMinAspectRatio[] = "minAspectRatio";
const char kMaxAspectRatio[] = "maxAspectRatio";
const char kMaxWidth[] = "maxWidth";
const char kMinWidth[] = "minWidth";
const char kMaxHeight[] = "maxHeight";
const char kMinHeight[] = "minHeight";
const char kMaxFrameRate[] = "maxFrameRate";
const char kMinFrameRate[] = "minFrameRate";
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] =
    "chromeMediaSourceId";                           // mapped to deviceId
const char kMediaStreamSourceInfoId[] = "sourceId";  // mapped to deviceId
const char kMediaStreamRenderToAssociatedSink[] =
    "chromeRenderToAssociatedSink";
// RenderToAssociatedSink will be going away some time.
const char kEchoCancellation[] = "echoCancellation";
const char kDisableLocalEcho[] = "disableLocalEcho";
const char kGoogEchoCancellation[] = "googEchoCancellation";
const char kGoogExperimentalEchoCancellation[] = "googEchoCancellation2";
const char kGoogAutoGainControl[] = "googAutoGainControl";
const char kGoogNoiseSuppression[] = "googNoiseSuppression";
const char kGoogExperimentalNoiseSuppression[] = "googNoiseSuppression2";
const char kGoogHighpassFilter[] = "googHighpassFilter";
const char kGoogAudioMirroring[] = "googAudioMirroring";
// Audio constraints.
const char kDAEchoCancellation[] = "googDAEchoCancellation";
// Google-specific constraint keys for a local video source (getUserMedia).
const char kNoiseReduction[] = "googNoiseReduction";

static bool ParseMandatoryConstraintsDictionary(
    const Dictionary& mandatory_constraints_dictionary,
    Vector<NameValueStringConstraint>& mandatory) {
  DummyExceptionStateForTesting exception_state;
  const HashMap<String, String>& mandatory_constraints_hash_map =
      mandatory_constraints_dictionary.GetOwnPropertiesAsStringHashMap(
          exception_state);
  if (exception_state.HadException())
    return false;

  for (const auto& iter : mandatory_constraints_hash_map)
    mandatory.push_back(NameValueStringConstraint(iter.key, iter.value));
  return true;
}

static bool ParseOptionalConstraintsVectorElement(
    const Dictionary& constraint,
    Vector<NameValueStringConstraint>& optional_constraints_vector) {
  DummyExceptionStateForTesting exception_state;
  const Vector<String>& local_names =
      constraint.GetPropertyNames(exception_state);
  if (exception_state.HadException() || local_names.size() != 1) {
    return false;
  }
  const String& key = local_names[0];
  std::optional<String> value = constraint.Get<IDLString>(key, exception_state);
  if (exception_state.HadException() || !value) {
    return false;
  }
  optional_constraints_vector.push_back(NameValueStringConstraint(key, *value));
  return true;
}

static bool Parse(const MediaTrackConstraints* constraints_in,
                  Vector<NameValueStringConstraint>& optional,
                  Vector<NameValueStringConstraint>& mandatory) {
  Vector<NameValueStringConstraint> mandatory_constraints_vector;
  if (constraints_in->hasMandatory()) {
    bool ok = ParseMandatoryConstraintsDictionary(
        Dictionary(constraints_in->mandatory()), mandatory);
    if (!ok)
      return false;
  }

  if (constraints_in->hasOptional()) {
    for (const auto& constraint : constraints_in->optional()) {
      bool ok = ParseOptionalConstraintsVectorElement(Dictionary(constraint),
                                                      optional);
      if (!ok)
        return false;
    }
  }
  return true;
}

static bool ToBoolean(const String& as_string) {
  return as_string == "true";
  // TODO(hta): Check against "false" and return error if it's neither.
  // https://crbug.com/576582
}

static void ParseOldStyleNames(
    ExecutionContext* context,
    const Vector<NameValueStringConstraint>& old_names,
    MediaTrackConstraintSetPlatform& result) {
  if (old_names.size() > 0) {
    UseCounter::Count(context, WebFeature::kOldConstraintsParsed);
  }
  for (const NameValueStringConstraint& constraint : old_names) {
    if (constraint.name_ == kMinAspectRatio) {
      result.aspect_ratio.SetMin(atof(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMaxAspectRatio) {
      result.aspect_ratio.SetMax(atof(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMaxWidth) {
      result.width.SetMax(atoi(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMinWidth) {
      result.width.SetMin(atoi(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMaxHeight) {
      result.height.SetMax(atoi(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMinHeight) {
      result.height.SetMin(atoi(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMinFrameRate) {
      result.frame_rate.SetMin(atof(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kMaxFrameRate) {
      result.frame_rate.SetMax(atof(constraint.value_.Utf8().c_str()));
    } else if (constraint.name_ == kEchoCancellation) {
      result.echo_cancellation.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kMediaStreamSource) {
      // TODO(hta): This has only a few legal values. Should be
      // represented as an enum, and cause type errors.
      // https://crbug.com/576582
      result.media_stream_source.SetExact(constraint.value_);
    } else if (constraint.name_ == kDisableLocalEcho &&
               RuntimeEnabledFeatures::
                   DesktopCaptureDisableLocalEchoControlEnabled()) {
      result.disable_local_echo.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kMediaStreamSourceId ||
               constraint.name_ == kMediaStreamSourceInfoId) {
      result.device_id.SetExact(constraint.value_);
    } else if (constraint.name_ == kMediaStreamRenderToAssociatedSink) {
      // TODO(hta): This is a boolean represented as string.
      // Should give TypeError when it's not parseable.
      // https://crbug.com/576582
      result.render_to_associated_sink.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogEchoCancellation) {
      result.goog_echo_cancellation.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogExperimentalEchoCancellation) {
      result.goog_experimental_echo_cancellation.SetExact(
          ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogAutoGainControl) {
      result.goog_auto_gain_control.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogNoiseSuppression) {
      result.goog_noise_suppression.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogExperimentalNoiseSuppression) {
      result.goog_experimental_noise_suppression.SetExact(
          ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogHighpassFilter) {
      result.goog_highpass_filter.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kGoogAudioMirroring) {
      result.goog_audio_mirroring.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kDAEchoCancellation) {
      result.goog_da_echo_cancellation.SetExact(ToBoolean(constraint.value_));
    } else if (constraint.name_ == kNoiseReduction) {
      result.goog_noise_reduction.SetExact(ToBoolean(constraint.value_));
    }
    // else: Nothing. Unrecognized constraints are simply ignored.
  }
}

static MediaConstraints CreateFromNamedConstraints(
    ExecutionContext* context,
    Vector<NameValueStringConstraint>& mandatory,
    const Vector<NameValueStringConstraint>& optional) {
  MediaTrackConstraintSetPlatform basic;
  MediaTrackConstraintSetPlatform advanced;
  MediaConstraints constraints;
  ParseOldStyleNames(context, mandatory, basic);
  // We ignore unknown names and syntax errors in optional constraints.
  Vector<MediaTrackConstraintSetPlatform> advanced_vector;
  for (const auto& optional_constraint : optional) {
    MediaTrackConstraintSetPlatform advanced_element;
    Vector<NameValueStringConstraint> element_as_list(1, optional_constraint);
    ParseOldStyleNames(context, element_as_list, advanced_element);
    if (!advanced_element.IsUnconstrained())
      advanced_vector.push_back(advanced_element);
  }
  constraints.Initialize(basic, advanced_vector);
  return constraints;
}

void CopyLongConstraint(const V8ConstrainLong* blink_union_form,
                        NakedValueDisposition naked_treatment,
                        LongConstraint& web_form) {
  web_form.SetIsPresent(true);
  switch (blink_union_form->GetContentType()) {
    case V8ConstrainLong::ContentType::kConstrainLongRange: {
      const auto* blink_form = blink_union_form->GetAsConstrainLongRange();
      if (blink_form->hasMin()) {
        web_form.SetMin(blink_form->min());
      }
      if (blink_form->hasMax()) {
        web_form.SetMax(blink_form->max());
      }
      if (blink_form->hasIdeal()) {
        web_form.SetIdeal(blink_form->ideal());
      }
      if (blink_form->hasExact()) {
        web_form.SetExact(blink_form->exact());
      }
      break;
    }
    case V8ConstrainLong::ContentType::kLong:
      switch (naked_treatment) {
        case NakedValueDisposition::kTreatAsIdeal:
          web_form.SetIdeal(blink_union_form->GetAsLong());
          break;
        case NakedValueDisposition::kTreatAsExact:
          web_form.SetExact(blink_union_form->GetAsLong());
          break;
      }
      break;
  }
}

void CopyDoubleConstraint(const V8ConstrainDouble* blink_union_form,
                          NakedValueDisposition naked_treatment,
                          DoubleConstraint& web_form) {
  web_form.SetIsPresent(true);
  switch (blink_union_form->GetContentType()) {
    case V8ConstrainDouble::ContentType::kConstrainDoubleRange: {
      const auto* blink_form = blink_union_form->GetAsConstrainDoubleRange();
      if (blink_form->hasMin()) {
        web_form.SetMin(blink_form->min());
      }
      if (blink_form->hasMax()) {
        web_form.SetMax(blink_form->max());
      }
      if (blink_form->hasIdeal()) {
        web_form.SetIdeal(blink_form->ideal());
      }
      if (blink_form->hasExact()) {
        web_form.SetExact(blink_form->exact());
      }
      break;
    }
    case V8ConstrainDouble::ContentType::kDouble:
      switch (naked_treatment) {
        case NakedValueDisposition::kTreatAsIdeal:
          web_form.SetIdeal(blink_union_form->GetAsDouble());
          break;
        case NakedValueDisposition::kTreatAsExact:
          web_form.SetExact(blink_union_form->GetAsDouble());
          break;
      }
      break;
  }
}

void CopyBooleanOrDoubleConstraint(
    const V8UnionBooleanOrConstrainDouble* blink_union_form,
    NakedValueDisposition naked_treatment,
    DoubleConstraint& web_form) {
  switch (blink_union_form->GetContentType()) {
    case V8UnionBooleanOrConstrainDouble::ContentType::kBoolean:
      web_form.SetIsPresent(blink_union_form->GetAsBoolean());
      break;
    case V8UnionBooleanOrConstrainDouble::ContentType::kConstrainDoubleRange:
    case V8UnionBooleanOrConstrainDouble::ContentType::kDouble:
      CopyDoubleConstraint(blink_union_form->GetAsV8ConstrainDouble(),
                           naked_treatment, web_form);
      break;
  }
}

bool ValidateString(const String& str, String& error_message) {
  if (str.length() > kMaxConstraintStringLength) {
    error_message = "Constraint string too long.";
    return false;
  }
  return true;
}

bool ValidateStringSeq(const Vector<String>& strs, String& error_message) {
  if (strs.size() > kMaxConstraintStringSeqLength) {
    error_message = "Constraint string sequence too long.";
    return false;
  }

  for (const String& str : strs) {
    if (!ValidateString(str, error_message)) {
      return false;
    }
  }

  return true;
}

bool ValidateStringConstraint(
    V8UnionStringOrStringSequence* string_or_string_seq,
    String& error_message) {
  switch (string_or_string_seq->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString: {
      return ValidateString(string_or_string_seq->GetAsString(), error_message);
    }
    case V8UnionStringOrStringSequence::ContentType::kStringSequence: {
      return ValidateStringSeq(string_or_string_seq->GetAsStringSequence(),
                               error_message);
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool ValidateStringConstraint(const V8ConstrainDOMString* blink_union_form,
                              String& error_message) {
  switch (blink_union_form->GetContentType()) {
    case V8ConstrainDOMString::ContentType::kConstrainDOMStringParameters: {
      const auto* blink_form =
          blink_union_form->GetAsConstrainDOMStringParameters();
      if (blink_form->hasIdeal() &&
          !ValidateStringConstraint(blink_form->ideal(), error_message)) {
        return false;
      }
      if (blink_form->hasExact() &&
          !ValidateStringConstraint(blink_form->exact(), error_message)) {
        return false;
      }
      return true;
    }
    case V8ConstrainDOMString::ContentType::kString:
      return ValidateString(blink_union_form->GetAsString(), error_message);
    case V8ConstrainDOMString::ContentType::kStringSequence:
      return ValidateStringSeq(blink_union_form->GetAsStringSequence(),
                               error_message);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

[[nodiscard]] bool ValidateAndCopyStringConstraint(
    const V8ConstrainDOMString* blink_union_form,
    NakedValueDisposition naked_treatment,
    StringConstraint& web_form,
    String& error_message) {
  if (!ValidateStringConstraint(blink_union_form, error_message)) {
    return false;
  }
  web_form.SetIsPresent(true);
  switch (blink_union_form->GetContentType()) {
    case V8ConstrainDOMString::ContentType::kConstrainDOMStringParameters: {
      const auto* blink_form =
          blink_union_form->GetAsConstrainDOMStringParameters();
      if (blink_form->hasIdeal()) {
        switch (blink_form->ideal()->GetContentType()) {
          case V8UnionStringOrStringSequence::ContentType::kString:
            web_form.SetIdeal(
                Vector<String>(1, blink_form->ideal()->GetAsString()));
            break;
          case V8UnionStringOrStringSequence::ContentType::kStringSequence:
            web_form.SetIdeal(blink_form->ideal()->GetAsStringSequence());
            break;
        }
      }
      if (blink_form->hasExact()) {
        switch (blink_form->exact()->GetContentType()) {
          case V8UnionStringOrStringSequence::ContentType::kString:
            web_form.SetExact(
                Vector<String>(1, blink_form->exact()->GetAsString()));
            break;
          case V8UnionStringOrStringSequence::ContentType::kStringSequence:
            web_form.SetExact(blink_form->exact()->GetAsStringSequence());
            break;
        }
      }
      break;
    }
    case V8ConstrainDOMString::ContentType::kString:
      switch (naked_treatment) {
        case NakedValueDisposition::kTreatAsIdeal:
          web_form.SetIdeal(Vector<String>(1, blink_union_form->GetAsString()));
          break;
        case NakedValueDisposition::kTreatAsExact:
          web_form.SetExact(Vector<String>(1, blink_union_form->GetAsString()));
          break;
      }
      break;
    case V8ConstrainDOMString::ContentType::kStringSequence:
      switch (naked_treatment) {
        case NakedValueDisposition::kTreatAsIdeal:
          web_form.SetIdeal(blink_union_form->GetAsStringSequence());
          break;
        case NakedValueDisposition::kTreatAsExact:
          web_form.SetExact(blink_union_form->GetAsStringSequence());
          break;
      }
      break;
  }
  return true;
}

void CopyBooleanConstraint(const V8ConstrainBoolean* blink_union_form,
                           NakedValueDisposition naked_treatment,
                           BooleanConstraint& web_form) {
  web_form.SetIsPresent(true);
  switch (blink_union_form->GetContentType()) {
    case V8ConstrainBoolean::ContentType::kBoolean:
      switch (naked_treatment) {
        case NakedValueDisposition::kTreatAsIdeal:
          web_form.SetIdeal(blink_union_form->GetAsBoolean());
          break;
        case NakedValueDisposition::kTreatAsExact:
          web_form.SetExact(blink_union_form->GetAsBoolean());
          break;
      }
      break;
    case V8ConstrainBoolean::ContentType::kConstrainBooleanParameters: {
      const auto* blink_form =
          blink_union_form->GetAsConstrainBooleanParameters();
      if (blink_form->hasIdeal()) {
        web_form.SetIdeal(blink_form->ideal());
      }
      if (blink_form->hasExact()) {
        web_form.SetExact(blink_form->exact());
      }
      break;
    }
  }
}

bool ValidateAndCopyConstraintSet(
    const MediaTrackConstraintSet* constraints_in,
    NakedValueDisposition naked_treatment,
    MediaTrackConstraintSetPlatform& constraint_buffer,
    String& error_message) {
  if (constraints_in->hasWidth()) {
    CopyLongConstraint(constraints_in->width(), naked_treatment,
                       constraint_buffer.width);
  }

  if (constraints_in->hasHeight()) {
    CopyLongConstraint(constraints_in->height(), naked_treatment,
                       constraint_buffer.height);
  }

  if (constraints_in->hasAspectRatio()) {
    CopyDoubleConstraint(constraints_in->aspectRatio(), naked_treatment,
                         constraint_buffer.aspect_ratio);
  }

  if (constraints_in->hasFrameRate()) {
    CopyDoubleConstraint(constraints_in->frameRate(), naked_treatment,
                         constraint_buffer.frame_rate);
  }

  if (constraints_in->hasFacingMode()) {
    if (!ValidateAndCopyStringConstraint(
            constraints_in->facingMode(), naked_treatment,
            constraint_buffer.facing_mode, error_message)) {
      return false;
    }
  }

  if (constraints_in->hasResizeMode()) {
    if (!ValidateAndCopyStringConstraint(
            constraints_in->resizeMode(), naked_treatment,
            constraint_buffer.resize_mode, error_message)) {
      return false;
    }
  }

  if (constraints_in->hasSampleRate()) {
    CopyLongConstraint(constraints_in->sampleRate(), naked_treatment,
                       constraint_buffer.sample_rate);
  }

  if (constraints_in->hasSampleSize()) {
    CopyLongConstraint(constraints_in->sampleSize(), naked_treatment,
                       constraint_buffer.sample_size);
  }

  if (constraints_in->hasEchoCancellation()) {
    CopyBooleanConstraint(constraints_in->echoCancellation(), naked_treatment,
                          constraint_buffer.echo_cancellation);
  }

  if (constraints_in->hasAutoGainControl()) {
    CopyBooleanConstraint(constraints_in->autoGainControl(), naked_treatment,
                          constraint_buffer.goog_auto_gain_control);
  }

  if (constraints_in->hasNoiseSuppression()) {
    CopyBooleanConstraint(constraints_in->noiseSuppression(), naked_treatment,
                          constraint_buffer.goog_noise_suppression);
  }

  if (constraints_in->hasVoiceIsolation()) {
    CopyBooleanConstraint(constraints_in->voiceIsolation(), naked_treatment,
                          constraint_buffer.voice_isolation);
  }

  if (constraints_in->hasLatency()) {
    CopyDoubleConstraint(constraints_in->latency(), naked_treatment,
                         constraint_buffer.latency);
  }

  if (constraints_in->hasChannelCount()) {
    CopyLongConstraint(constraints_in->channelCount(), naked_treatment,
                       constraint_buffer.channel_count);
  }

  if (constraints_in->hasDeviceId()) {
    if (!ValidateAndCopyStringConstraint(
            constraints_in->deviceId(), naked_treatment,
            constraint_buffer.device_id, error_message)) {
      return false;
    }
  }

  if (constraints_in->hasGroupId()) {
    if (!ValidateAndCopyStringConstraint(
            constraints_in->groupId(), naked_treatment,
            constraint_buffer.group_id, error_message)) {
      return false;
    }
  }

  if (constraints_in->hasExposureCompensation()) {
    CopyDoubleConstraint(constraints_in->exposureCompensation(),
                         naked_treatment,
                         constraint_buffer.exposure_compensation);
  }

  if (constraints_in->hasExposureTime()) {
    CopyDoubleConstraint(constraints_in->exposureTime(), naked_treatment,
                         constraint_buffer.exposure_time);
  }

  if (constraints_in->hasColorTemperature()) {
    CopyDoubleConstraint(constraints_in->colorTemperature(), naked_treatment,
                         constraint_buffer.color_temperature);
  }

  if (constraints_in->hasIso()) {
    CopyDoubleConstraint(constraints_in->iso(), naked_treatment,
                         constraint_buffer.iso);
  }

  if (constraints_in->hasBrightness()) {
    CopyDoubleConstraint(constraints_in->brightness(), naked_treatment,
                         constraint_buffer.brightness);
  }

  if (constraints_in->hasContrast()) {
    CopyDoubleConstraint(constraints_in->contrast(), naked_treatment,
                         constraint_buffer.contrast);
  }

  if (constraints_in->hasSaturation()) {
    CopyDoubleConstraint(constraints_in->saturation(), naked_treatment,
                         constraint_buffer.saturation);
  }

  if (constraints_in->hasSharpness()) {
    CopyDoubleConstraint(constraints_in->sharpness(), naked_treatment,
                         constraint_buffer.sharpness);
  }

  if (constraints_in->hasFocusDistance()) {
    CopyDoubleConstraint(constraints_in->focusDistance(), naked_treatment,
                         constraint_buffer.focus_distance);
  }

  if (constraints_in->hasPan()) {
    CopyBooleanOrDoubleConstraint(constraints_in->pan(), naked_treatment,
                                  constraint_buffer.pan);
  }

  if (constraints_in->hasTilt()) {
    CopyBooleanOrDoubleConstraint(constraints_in->tilt(), naked_treatment,
                                  constraint_buffer.tilt);
  }

  if (constraints_in->hasZoom()) {
    CopyBooleanOrDoubleConstraint(constraints_in->zoom(), naked_treatment,
                                  constraint_buffer.zoom);
  }

  if (constraints_in->hasTorch()) {
    CopyBooleanConstraint(constraints_in->torch(), naked_treatment,
                          constraint_buffer.torch);
  }

  if (constraints_in->hasBackgroundBlur()) {
    CopyBooleanConstraint(constraints_in->backgroundBlur(), naked_treatment,
                          constraint_buffer.background_blur);
  }

  if (constraints_in->hasBackgroundSegmentationMask()) {
    CopyBooleanConstraint(constraints_in->backgroundSegmentationMask(),
                          naked_treatment,
                          constraint_buffer.background_segmentation_mask);
  }

  if (constraints_in->hasEyeGazeCorrection()) {
    CopyBooleanConstraint(constraints_in->eyeGazeCorrection(), naked_treatment,
                          constraint_buffer.eye_gaze_correction);
  }

  if (constraints_in->hasFaceFraming()) {
    CopyBooleanConstraint(constraints_in->faceFraming(), naked_treatment,
                          constraint_buffer.face_framing);
  }

  if (constraints_in->hasDisplaySurface()) {
    if (!ValidateAndCopyStringConstraint(
            constraints_in->displaySurface(), naked_treatment,
            constraint_buffer.display_surface, error_message)) {
      return false;
    }
  }

  if (constraints_in->hasSuppressLocalAudioPlayback()) {
    CopyBooleanConstraint(constraints_in->suppressLocalAudioPlayback(),
                          naked_treatment,
                          constraint_buffer.suppress_local_audio_playback);
  }
  return true;
}

template <class T>
bool UseNakedNumeric(const T& input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.HasIdeal() &&
             !(input.HasExact() || input.HasMin() || input.HasMax());
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.HasExact() &&
             !(input.HasIdeal() || input.HasMin() || input.HasMax());
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

template <class T>
bool UseNakedNonNumeric(const T& input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.HasIdeal() && !input.HasExact();
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.HasExact() && !input.HasIdeal();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

template <typename U, class T>
U GetNakedValue(const T& input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.Ideal();
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.Exact();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return input.Exact();
}

V8ConstrainLong* ConvertLong(const LongConstraint& input,
                             NakedValueDisposition naked_treatment) {
  if (UseNakedNumeric(input, naked_treatment)) {
    return MakeGarbageCollected<V8ConstrainLong>(
        GetNakedValue<uint32_t>(input, naked_treatment));
  } else if (!input.IsUnconstrained()) {
    ConstrainLongRange* output = ConstrainLongRange::Create();
    if (input.HasExact())
      output->setExact(input.Exact());
    if (input.HasMin())
      output->setMin(input.Min());
    if (input.HasMax())
      output->setMax(input.Max());
    if (input.HasIdeal())
      output->setIdeal(input.Ideal());
    return MakeGarbageCollected<V8ConstrainLong>(output);
  }
  return nullptr;
}

V8ConstrainDouble* ConvertDouble(const DoubleConstraint& input,
                                 NakedValueDisposition naked_treatment) {
  if (UseNakedNumeric(input, naked_treatment)) {
    return MakeGarbageCollected<V8ConstrainDouble>(
        GetNakedValue<double>(input, naked_treatment));
  } else if (!input.IsUnconstrained()) {
    ConstrainDoubleRange* output = ConstrainDoubleRange::Create();
    if (input.HasExact())
      output->setExact(input.Exact());
    if (input.HasIdeal())
      output->setIdeal(input.Ideal());
    if (input.HasMin())
      output->setMin(input.Min());
    if (input.HasMax())
      output->setMax(input.Max());
    return MakeGarbageCollected<V8ConstrainDouble>(output);
  }
  return nullptr;
}

V8UnionBooleanOrConstrainDouble* ConvertBooleanOrDouble(
    const DoubleConstraint& input,
    NakedValueDisposition naked_treatment) {
  if (UseNakedNumeric(input, naked_treatment)) {
    return MakeGarbageCollected<V8UnionBooleanOrConstrainDouble>(
        GetNakedValue<double>(input, naked_treatment));
  } else if (!input.IsUnconstrained()) {
    ConstrainDoubleRange* output = ConstrainDoubleRange::Create();
    if (input.HasExact())
      output->setExact(input.Exact());
    if (input.HasIdeal())
      output->setIdeal(input.Ideal());
    if (input.HasMin())
      output->setMin(input.Min());
    if (input.HasMax())
      output->setMax(input.Max());
    return MakeGarbageCollected<V8UnionBooleanOrConstrainDouble>(output);
  }
  return nullptr;
}

V8UnionStringOrStringSequence* ConvertStringSequence(
    const Vector<String>& input) {
  if (input.size() > 1) {
    return MakeGarbageCollected<V8UnionStringOrStringSequence>(input);
  } else if (!input.empty()) {
    return MakeGarbageCollected<V8UnionStringOrStringSequence>(input[0]);
  }
  return nullptr;
}

V8ConstrainDOMString* ConvertString(const StringConstraint& input,
                                    NakedValueDisposition naked_treatment) {
  if (UseNakedNonNumeric(input, naked_treatment)) {
    const Vector<String>& input_buffer(
        GetNakedValue<const Vector<String>&>(input, naked_treatment));
    if (input_buffer.size() > 1) {
      return MakeGarbageCollected<V8ConstrainDOMString>(input_buffer);
    } else if (!input_buffer.empty()) {
      return MakeGarbageCollected<V8ConstrainDOMString>(input_buffer[0]);
    }
    return nullptr;
  } else if (!input.IsUnconstrained()) {
    ConstrainDOMStringParameters* output =
        ConstrainDOMStringParameters::Create();
    if (input.HasExact())
      output->setExact(ConvertStringSequence(input.Exact()));
    if (input.HasIdeal())
      output->setIdeal(ConvertStringSequence(input.Ideal()));
    return MakeGarbageCollected<V8ConstrainDOMString>(output);
  }
  return nullptr;
}

V8ConstrainBoolean* ConvertBoolean(const BooleanConstraint& input,
                                   NakedValueDisposition naked_treatment) {
  if (UseNakedNonNumeric(input, naked_treatment)) {
    return MakeGarbageCollected<V8ConstrainBoolean>(
        GetNakedValue<bool>(input, naked_treatment));
  } else if (!input.IsUnconstrained()) {
    ConstrainBooleanParameters* output = ConstrainBooleanParameters::Create();
    if (input.HasExact())
      output->setExact(input.Exact());
    if (input.HasIdeal())
      output->setIdeal(input.Ideal());
    return MakeGarbageCollected<V8ConstrainBoolean>(output);
  }
  return nullptr;
}

void ConvertConstraintSet(const MediaTrackConstraintSetPlatform& input,
                          NakedValueDisposition naked_treatment,
                          MediaTrackConstraintSet* output) {
  if (!input.width.IsUnconstrained())
    output->setWidth(ConvertLong(input.width, naked_treatment));
  if (!input.height.IsUnconstrained())
    output->setHeight(ConvertLong(input.height, naked_treatment));
  if (!input.aspect_ratio.IsUnconstrained())
    output->setAspectRatio(ConvertDouble(input.aspect_ratio, naked_treatment));
  if (!input.frame_rate.IsUnconstrained())
    output->setFrameRate(ConvertDouble(input.frame_rate, naked_treatment));
  if (!input.facing_mode.IsUnconstrained())
    output->setFacingMode(ConvertString(input.facing_mode, naked_treatment));
  if (!input.resize_mode.IsUnconstrained())
    output->setResizeMode(ConvertString(input.resize_mode, naked_treatment));
  if (!input.sample_rate.IsUnconstrained())
    output->setSampleRate(ConvertLong(input.sample_rate, naked_treatment));
  if (!input.sample_size.IsUnconstrained())
    output->setSampleSize(ConvertLong(input.sample_size, naked_treatment));
  if (!input.echo_cancellation.IsUnconstrained()) {
    output->setEchoCancellation(
        ConvertBoolean(input.echo_cancellation, naked_treatment));
  }
  if (!input.goog_auto_gain_control.IsUnconstrained()) {
    output->setAutoGainControl(
        ConvertBoolean(input.goog_auto_gain_control, naked_treatment));
  }
  if (!input.goog_noise_suppression.IsUnconstrained()) {
    output->setNoiseSuppression(
        ConvertBoolean(input.goog_noise_suppression, naked_treatment));
  }
  if (!input.voice_isolation.IsUnconstrained()) {
    output->setVoiceIsolation(
        ConvertBoolean(input.voice_isolation, naked_treatment));
  }
  if (!input.latency.IsUnconstrained())
    output->setLatency(ConvertDouble(input.latency, naked_treatment));
  if (!input.channel_count.IsUnconstrained())
    output->setChannelCount(ConvertLong(input.channel_count, naked_treatment));
  if (!input.device_id.IsUnconstrained())
    output->setDeviceId(ConvertString(input.device_id, naked_treatment));
  if (!input.group_id.IsUnconstrained())
    output->setGroupId(ConvertString(input.group_id, naked_treatment));
  if (!input.exposure_compensation.IsUnconstrained()) {
    output->setExposureCompensation(
        ConvertDouble(input.exposure_compensation, naked_treatment));
  }
  if (!input.exposure_time.IsUnconstrained()) {
    output->setExposureTime(
        ConvertDouble(input.exposure_time, naked_treatment));
  }
  if (!input.color_temperature.IsUnconstrained()) {
    output->setColorTemperature(
        ConvertDouble(input.color_temperature, naked_treatment));
  }
  if (!input.iso.IsUnconstrained()) {
    output->setIso(ConvertDouble(input.iso, naked_treatment));
  }
  if (!input.brightness.IsUnconstrained()) {
    output->setBrightness(ConvertDouble(input.brightness, naked_treatment));
  }
  if (!input.contrast.IsUnconstrained()) {
    output->setContrast(ConvertDouble(input.contrast, naked_treatment));
  }
  if (!input.saturation.IsUnconstrained()) {
    output->setSaturation(ConvertDouble(input.saturation, naked_treatment));
  }
  if (!input.sharpness.IsUnconstrained()) {
    output->setSharpness(ConvertDouble(input.sharpness, naked_treatment));
  }
  if (!input.focus_distance.IsUnconstrained()) {
    output->setFocusDistance(
        ConvertDouble(input.focus_distance, naked_treatment));
  }
  if (!input.pan.IsUnconstrained())
    output->setPan(ConvertBooleanOrDouble(input.pan, naked_treatment));
  if (!input.tilt.IsUnconstrained())
    output->setTilt(ConvertBooleanOrDouble(input.tilt, naked_treatment));
  if (!input.zoom.IsUnconstrained())
    output->setZoom(ConvertBooleanOrDouble(input.zoom, naked_treatment));
  if (!input.torch.IsUnconstrained()) {
    output->setTorch(ConvertBoolean(input.torch, naked_treatment));
  }
  if (!input.background_blur.IsUnconstrained()) {
    output->setBackgroundBlur(
        ConvertBoolean(input.background_blur, naked_treatment));
  }
  if (!input.background_segmentation_mask.IsUnconstrained()) {
    output->setBackgroundSegmentationMask(
        ConvertBoolean(input.background_segmentation_mask, naked_treatment));
  }
  if (!input.eye_gaze_correction.IsUnconstrained()) {
    output->setEyeGazeCorrection(
        ConvertBoolean(input.eye_gaze_correction, naked_treatment));
  }
  if (!input.face_framing.IsUnconstrained()) {
    output->setFaceFraming(ConvertBoolean(input.face_framing, naked_treatment));
  }
  if (!input.suppress_local_audio_playback.IsUnconstrained()) {
    output->setSuppressLocalAudioPlayback(
        ConvertBoolean(input.suppress_local_audio_playback, naked_treatment));
  }
  // TODO(hta): Decide the future of the nonstandard constraints.
  // If they go forward, they need to be added here.
  // https://crbug.com/605673
}

}  // namespace

MediaConstraints ConvertTrackConstraintsToMediaConstraints(
    const MediaTrackConstraints* constraints_in,
    String& error_message) {
  MediaTrackConstraintSetPlatform constraint_buffer;
  Vector<MediaTrackConstraintSetPlatform> advanced_buffer;
  if (!ValidateAndCopyConstraintSet(constraints_in,
                                    NakedValueDisposition::kTreatAsIdeal,
                                    constraint_buffer, error_message)) {
    return MediaConstraints();
  }
  if (constraints_in->hasAdvanced()) {
    for (const auto& element : constraints_in->advanced()) {
      MediaTrackConstraintSetPlatform advanced_element;
      if (!ValidateAndCopyConstraintSet(element,
                                        NakedValueDisposition::kTreatAsExact,
                                        advanced_element, error_message)) {
        return MediaConstraints();
      }
      advanced_buffer.push_back(advanced_element);
    }
  }
  MediaConstraints constraints;
  constraints.Initialize(constraint_buffer, advanced_buffer);
  return constraints;
}

MediaConstraints Create(ExecutionContext* context,
                        const MediaTrackConstraints* constraints_in,
                        String& error_message) {
  MediaConstraints standard_form =
      ConvertTrackConstraintsToMediaConstraints(constraints_in, error_message);
  if (standard_form.IsNull()) {
    return standard_form;
  }
  if (constraints_in->hasOptional() || constraints_in->hasMandatory()) {
    if (!standard_form.IsUnconstrained()) {
      UseCounter::Count(context, WebFeature::kMediaStreamConstraintsOldAndNew);
      error_message =
          "Malformed constraint: Cannot use both optional/mandatory and "
          "specific or advanced constraints.";
      return MediaConstraints();
    }
    Vector<NameValueStringConstraint> optional;
    Vector<NameValueStringConstraint> mandatory;
    if (!Parse(constraints_in, optional, mandatory)) {
      error_message = "Malformed constraints object.";
      return MediaConstraints();
    }
    UseCounter::Count(context, WebFeature::kMediaStreamConstraintsNameValue);
    return CreateFromNamedConstraints(context, mandatory, optional);
  }
  UseCounter::Count(context, WebFeature::kMediaStreamConstraintsConformant);
  return standard_form;
}

MediaConstraints Create() {
  MediaConstraints constraints;
  constraints.Initialize();
  return constraints;
}

MediaTrackConstraints* ConvertConstraints(const MediaConstraints& input) {
  MediaTrackConstraints* output = MediaTrackConstraints::Create();
  if (input.IsNull())
    return output;
  ConvertConstraintSet(input.Basic(), NakedValueDisposition::kTreatAsIdeal,
                       output);

  HeapVector<Member<MediaTrackConstraintSet>> advanced_vector;
  for (const auto& it : input.Advanced()) {
    if (it.IsUnconstrained())
      continue;
    MediaTrackConstraintSet* element = MediaTrackConstraintSet::Create();
    ConvertConstraintSet(it, NakedValueDisposition::kTreatAsExact, element);
    advanced_vector.push_back(element);
  }
  if (!advanced_vector.empty())
    output->setAdvanced(advanced_vector);

  return output;
}

}  // namespace media_constraints_impl
}  // namespace blink
