// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_fill_light_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_settings_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_photo_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainpoint2dparameters_point2dsequence.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture_frame_grabber.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using BackgroundBlurMode = media::mojom::blink::BackgroundBlurMode;
using FillLightMode = media::mojom::blink::FillLightMode;
using MeteringMode = media::mojom::blink::MeteringMode;
using RedEyeReduction = media::mojom::blink::RedEyeReduction;

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

const char kInvalidStateTrackError[] =
    "The associated Track is in an invalid state";

using CopyPanTiltZoom = base::StrongAlias<class CopyPanTiltZoomTag, bool>;

template <typename T>
void CopyCommonMembers(const T* source,
                       T* destination,
                       CopyPanTiltZoom copy_pan_tilt_zoom) {
  DCHECK(source);
  DCHECK(destination);
  // Merge any present |source| common members into |destination|.
  if (source->hasWhiteBalanceMode()) {
    destination->setWhiteBalanceMode(source->whiteBalanceMode());
  }
  if (source->hasExposureMode()) {
    destination->setExposureMode(source->exposureMode());
  }
  if (source->hasFocusMode()) {
    destination->setFocusMode(source->focusMode());
  }
  if (source->hasExposureCompensation()) {
    destination->setExposureCompensation(source->exposureCompensation());
  }
  if (source->hasExposureTime()) {
    destination->setExposureTime(source->exposureTime());
  }
  if (source->hasColorTemperature()) {
    destination->setColorTemperature(source->colorTemperature());
  }
  if (source->hasIso()) {
    destination->setIso(source->iso());
  }
  if (source->hasBrightness()) {
    destination->setBrightness(source->brightness());
  }
  if (source->hasContrast()) {
    destination->setContrast(source->contrast());
  }
  if (source->hasSaturation()) {
    destination->setSaturation(source->saturation());
  }
  if (source->hasSharpness()) {
    destination->setSharpness(source->sharpness());
  }
  if (source->hasFocusDistance()) {
    destination->setFocusDistance(source->focusDistance());
  }
  if (copy_pan_tilt_zoom) {
    if (source->hasPan()) {
      destination->setPan(source->pan());
    }
    if (source->hasTilt()) {
      destination->setTilt(source->tilt());
    }
    if (source->hasZoom()) {
      destination->setZoom(source->zoom());
    }
  }
  if (source->hasTorch()) {
    destination->setTorch(source->torch());
  }
  if (source->hasBackgroundBlur()) {
    destination->setBackgroundBlur(source->backgroundBlur());
  }
}

void CopyCapabilities(const MediaTrackCapabilities* source,
                      MediaTrackCapabilities* destination,
                      CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
}

void CopyConstraintSet(const MediaTrackConstraintSet* source,
                       MediaTrackConstraintSet* destination,
                       CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
  if (source->hasPointsOfInterest()) {
    destination->setPointsOfInterest(source->pointsOfInterest());
  }
}

void CopySettings(const MediaTrackSettings* source,
                  MediaTrackSettings* destination,
                  CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
  if (source->hasPointsOfInterest() && !source->pointsOfInterest().empty()) {
    destination->setPointsOfInterest(source->pointsOfInterest());
  }
}

bool TrackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  return track.readyState() != "live" || !track.enabled() || track.muted();
}

MeteringMode ParseMeteringMode(const String& blink_mode) {
  if (blink_mode == "manual")
    return MeteringMode::MANUAL;
  if (blink_mode == "single-shot")
    return MeteringMode::SINGLE_SHOT;
  if (blink_mode == "continuous")
    return MeteringMode::CONTINUOUS;
  if (blink_mode == "none")
    return MeteringMode::NONE;
  NOTREACHED();
  return MeteringMode::NONE;
}

FillLightMode ParseFillLightMode(const String& blink_mode) {
  if (blink_mode == "off")
    return FillLightMode::OFF;
  if (blink_mode == "auto")
    return FillLightMode::AUTO;
  if (blink_mode == "flash")
    return FillLightMode::FLASH;
  NOTREACHED();
  return FillLightMode::OFF;
}

bool ToBooleanMode(BackgroundBlurMode mode) {
  switch (mode) {
    case BackgroundBlurMode::OFF:
      return false;
    case BackgroundBlurMode::BLUR:
      return true;
  }
}

WebString ToString(MeteringMode value) {
  switch (value) {
    case MeteringMode::NONE:
      return WebString::FromUTF8("none");
    case MeteringMode::MANUAL:
      return WebString::FromUTF8("manual");
    case MeteringMode::SINGLE_SHOT:
      return WebString::FromUTF8("single-shot");
    case MeteringMode::CONTINUOUS:
      return WebString::FromUTF8("continuous");
  }
}

V8FillLightMode ToV8FillLightMode(FillLightMode value) {
  switch (value) {
    case FillLightMode::OFF:
      return V8FillLightMode(V8FillLightMode::Enum::kOff);
    case FillLightMode::AUTO:
      return V8FillLightMode(V8FillLightMode::Enum::kAuto);
    case FillLightMode::FLASH:
      return V8FillLightMode(V8FillLightMode::Enum::kFlash);
  }
}

WebString ToString(RedEyeReduction value) {
  switch (value) {
    case RedEyeReduction::NEVER:
      return WebString::FromUTF8("never");
    case RedEyeReduction::ALWAYS:
      return WebString::FromUTF8("always");
    case RedEyeReduction::CONTROLLABLE:
      return WebString::FromUTF8("controllable");
  }
}

MediaSettingsRange* ToMediaSettingsRange(
    const media::mojom::blink::Range& range) {
  MediaSettingsRange* result = MediaSettingsRange::Create();
  result->setMax(range.max);
  result->setMin(range.min);
  result->setStep(range.step);
  return result;
}

}  // anonymous namespace

ImageCapture* ImageCapture::Create(ExecutionContext* context,
                                   MediaStreamTrack* track,
                                   ExceptionState& exception_state) {
  if (track->kind() != "video") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot create an ImageCapturer from a non-video Track.");
    return nullptr;
  }

  // The initial PTZ permission comes from the internal ImageCapture object of
  // the track, if already created.
  bool pan_tilt_zoom_allowed =
      (track->GetImageCapture() &&
       track->GetImageCapture()->HasPanTiltZoomPermissionGranted());

  return MakeGarbageCollected<ImageCapture>(
      context, track, pan_tilt_zoom_allowed, base::DoNothing());
}

ImageCapture::~ImageCapture() {
  DCHECK(!HasEventListeners());
  // There should be no more outstanding |m_serviceRequests| at this point
  // since each of them holds a persistent handle to this object.
  DCHECK(service_requests_.empty());
}

const AtomicString& ImageCapture::InterfaceName() const {
  return event_target_names::kImageCapture;
}

ExecutionContext* ImageCapture::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool ImageCapture::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void ImageCapture::ContextDestroyed() {
  RemoveAllEventListeners();
  service_requests_.clear();
  DCHECK(!HasEventListeners());
}

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* script_state) {
  return GetMojoPhotoState(
      script_state, WTF::BindOnce(&ImageCapture::ResolveWithPhotoCapabilities,
                                  WrapPersistent(this)));
}

ScriptPromise ImageCapture::getPhotoSettings(ScriptState* script_state) {
  return GetMojoPhotoState(
      script_state, WTF::BindOnce(&ImageCapture::ResolveWithPhotoSettings,
                                  WrapPersistent(this)));
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state,
                                      const PhotoSettings* photo_settings) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::takePhoto", TRACE_EVENT_SCOPE_PROCESS);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  // TODO(mcasas): should be using a mojo::StructTraits instead.
  auto settings = media::mojom::blink::PhotoSettings::New();

  settings->has_height = photo_settings->hasImageHeight();
  if (settings->has_height) {
    const double height = photo_settings->imageHeight();
    if (photo_capabilities_ && photo_capabilities_->hasImageHeight() &&
        (height < photo_capabilities_->imageHeight()->min() ||
         height > photo_capabilities_->imageHeight()->max())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "imageHeight setting out of range"));
      return promise;
    }
    settings->height = height;
  }
  settings->has_width = photo_settings->hasImageWidth();
  if (settings->has_width) {
    const double width = photo_settings->imageWidth();
    if (photo_capabilities_ && photo_capabilities_->hasImageWidth() &&
        (width < photo_capabilities_->imageWidth()->min() ||
         width > photo_capabilities_->imageWidth()->max())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "imageWidth setting out of range"));
      return promise;
    }
    settings->width = width;
  }

  settings->has_red_eye_reduction = photo_settings->hasRedEyeReduction();
  if (settings->has_red_eye_reduction) {
    if (photo_capabilities_ && photo_capabilities_->hasRedEyeReduction() &&
        photo_capabilities_->redEyeReduction() !=
            V8RedEyeReduction::Enum::kControllable) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "redEyeReduction is not controllable."));
      return promise;
    }
    settings->red_eye_reduction = photo_settings->redEyeReduction();
  }

  settings->has_fill_light_mode = photo_settings->hasFillLightMode();
  if (settings->has_fill_light_mode) {
    const String fill_light_mode = photo_settings->fillLightMode();
    if (photo_capabilities_ && photo_capabilities_->hasFillLightMode() &&
        photo_capabilities_->fillLightMode().Find(fill_light_mode) ==
            kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported fillLightMode"));
      return promise;
    }
    settings->fill_light_mode = ParseFillLightMode(fill_light_mode);
  }

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/true));
  return promise;
}

ScriptPromise ImageCapture::grabFrame(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  // Create |m_frameGrabber| the first time.
  if (!frame_grabber_) {
    frame_grabber_ = std::make_unique<ImageCaptureFrameGrabber>();
  }

  if (!frame_grabber_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Couldn't create platform resources"));
    return promise;
  }

  auto resolver_callback_adapter =
      std::make_unique<CallbackPromiseAdapter<ImageBitmap, void>>(resolver);
  frame_grabber_->GrabFrame(stream_track_->Component(),
                            std::move(resolver_callback_adapter),
                            ExecutionContext::From(script_state)
                                ->GetTaskRunner(TaskType::kDOMManipulation));

  return promise;
}

void ImageCapture::GetMediaTrackCapabilities(
    MediaTrackCapabilities* capabilities) const {
  // Merge any present |capabilities_| members into |capabilities|.
  CopyCapabilities(capabilities_, capabilities,
                   CopyPanTiltZoom(HasPanTiltZoomPermissionGranted()));
}

// TODO(mcasas): make the implementation fully Spec compliant, see the TODOs
// inside the method, https://crbug.com/708723.
void ImageCapture::SetMediaTrackConstraints(
    ScriptPromiseResolver* resolver,
    const MediaTrackConstraints* all_constraints) {
  DCHECK(all_constraints);
  if (!all_constraints->hasAdvanced() || all_constraints->advanced().empty()) {
    // TODO(crbug.com/1408091): This is not spec compliant.
    // If there are no advanced constraints (but only required and optional
    // constraints), the required and optional constraints should be applied.
    ClearMediaTrackConstraints();
    resolver->Resolve();
    return;
  }

  const auto& constraints_vector = all_constraints->advanced();
  DCHECK_GT(constraints_vector.size(), 0u);
  // TODO(mcasas): add support more than one single advanced constraint.
  const MediaTrackConstraintSet* constraints = constraints_vector[0];

  ExecutionContext* context = GetExecutionContext();
  if (constraints->hasWhiteBalanceMode())
    UseCounter::Count(context, WebFeature::kImageCaptureWhiteBalanceMode);
  if (constraints->hasExposureMode())
    UseCounter::Count(context, WebFeature::kImageCaptureExposureMode);
  if (constraints->hasFocusMode())
    UseCounter::Count(context, WebFeature::kImageCaptureFocusMode);
  if (constraints->hasPointsOfInterest())
    UseCounter::Count(context, WebFeature::kImageCapturePointsOfInterest);
  if (constraints->hasExposureCompensation())
    UseCounter::Count(context, WebFeature::kImageCaptureExposureCompensation);
  if (constraints->hasExposureTime())
    UseCounter::Count(context, WebFeature::kImageCaptureExposureTime);
  if (constraints->hasColorTemperature())
    UseCounter::Count(context, WebFeature::kImageCaptureColorTemperature);
  if (constraints->hasIso())
    UseCounter::Count(context, WebFeature::kImageCaptureIso);
  if (constraints->hasBrightness())
    UseCounter::Count(context, WebFeature::kImageCaptureBrightness);
  if (constraints->hasContrast())
    UseCounter::Count(context, WebFeature::kImageCaptureContrast);
  if (constraints->hasSaturation())
    UseCounter::Count(context, WebFeature::kImageCaptureSaturation);
  if (constraints->hasSharpness())
    UseCounter::Count(context, WebFeature::kImageCaptureSharpness);
  if (constraints->hasFocusDistance())
    UseCounter::Count(context, WebFeature::kImageCaptureFocusDistance);
  if (constraints->hasPan())
    UseCounter::Count(context, WebFeature::kImageCapturePan);
  if (constraints->hasTilt())
    UseCounter::Count(context, WebFeature::kImageCaptureTilt);
  if (constraints->hasZoom())
    UseCounter::Count(context, WebFeature::kImageCaptureZoom);
  if (constraints->hasTorch())
    UseCounter::Count(context, WebFeature::kImageCaptureTorch);
  // TODO(eero.hakkinen@intel.com): count how many times backgroundBlur is
  // used.

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return;
  }

  if ((constraints->hasWhiteBalanceMode() &&
       !capabilities_->hasWhiteBalanceMode()) ||
      (constraints->hasExposureMode() && !capabilities_->hasExposureMode()) ||
      (constraints->hasFocusMode() && !capabilities_->hasFocusMode()) ||
      (constraints->hasExposureCompensation() &&
       !capabilities_->hasExposureCompensation()) ||
      (constraints->hasExposureTime() && !capabilities_->hasExposureTime()) ||
      (constraints->hasColorTemperature() &&
       !capabilities_->hasColorTemperature()) ||
      (constraints->hasIso() && !capabilities_->hasIso()) ||
      (constraints->hasBrightness() && !capabilities_->hasBrightness()) ||
      (constraints->hasContrast() && !capabilities_->hasContrast()) ||
      (constraints->hasSaturation() && !capabilities_->hasSaturation()) ||
      (constraints->hasSharpness() && !capabilities_->hasSharpness()) ||
      (constraints->hasFocusDistance() && !capabilities_->hasFocusDistance()) ||
      (constraints->hasPan() &&
       !(capabilities_->hasPan() && HasPanTiltZoomPermissionGranted())) ||
      (constraints->hasTilt() &&
       !(capabilities_->hasTilt() && HasPanTiltZoomPermissionGranted())) ||
      (constraints->hasZoom() &&
       !(capabilities_->hasZoom() && HasPanTiltZoomPermissionGranted())) ||
      (constraints->hasTorch() && !capabilities_->hasTorch()) ||
      (constraints->hasBackgroundBlur() &&
       !capabilities_->hasBackgroundBlur())) {
    // TODO(eero): supply a constraint name.
    resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
        "", "Unsupported constraint(s)"));
    return;
  }

  auto settings = media::mojom::blink::PhotoSettings::New();
  MediaTrackConstraintSet* temp_constraints =
      current_constraints_ ? current_constraints_.Get()
                           : MediaTrackConstraintSet::Create();

  // TODO(mcasas): support other Mode types beyond simple string i.e. the
  // equivalents of "sequence<DOMString>"" or "ConstrainDOMStringParameters".
  settings->has_white_balance_mode =
      constraints->hasWhiteBalanceMode() &&
      constraints->whiteBalanceMode()->IsString();
  if (settings->has_white_balance_mode) {
    const auto white_balance_mode =
        constraints->whiteBalanceMode()->GetAsString();
    if (capabilities_->whiteBalanceMode().Find(white_balance_mode) ==
        kNotFound) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "whiteBalanceMode", "Unsupported whiteBalanceMode."));
      return;
    }
    temp_constraints->setWhiteBalanceMode(constraints->whiteBalanceMode());
    settings->white_balance_mode = ParseMeteringMode(white_balance_mode);
  }
  settings->has_exposure_mode =
      constraints->hasExposureMode() && constraints->exposureMode()->IsString();
  if (settings->has_exposure_mode) {
    const auto exposure_mode = constraints->exposureMode()->GetAsString();
    if (capabilities_->exposureMode().Find(exposure_mode) == kNotFound) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "exposureMode", "Unsupported exposureMode."));
      return;
    }
    temp_constraints->setExposureMode(constraints->exposureMode());
    settings->exposure_mode = ParseMeteringMode(exposure_mode);
  }

  settings->has_focus_mode =
      constraints->hasFocusMode() && constraints->focusMode()->IsString();
  if (settings->has_focus_mode) {
    const auto focus_mode = constraints->focusMode()->GetAsString();
    if (capabilities_->focusMode().Find(focus_mode) == kNotFound) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "focusMode", "Unsupported focusMode."));
      return;
    }
    temp_constraints->setFocusMode(constraints->focusMode());
    settings->focus_mode = ParseMeteringMode(focus_mode);
  }

  // TODO(mcasas): support ConstrainPoint2DParameters.
  if (constraints->hasPointsOfInterest() &&
      constraints->pointsOfInterest()->IsPoint2DSequence()
  ) {
    for (const auto& point :
         constraints->pointsOfInterest()->GetAsPoint2DSequence()
    ) {
      auto mojo_point = media::mojom::blink::Point2D::New();
      mojo_point->x = point->x();
      mojo_point->y = point->y();
      settings->points_of_interest.push_back(std::move(mojo_point));
    }
    temp_constraints->setPointsOfInterest(constraints->pointsOfInterest());
  }

  // TODO(mcasas): support ConstrainDoubleRange where applicable.
  settings->has_exposure_compensation =
      constraints->hasExposureCompensation() &&
      constraints->exposureCompensation()->IsDouble();
  if (settings->has_exposure_compensation) {
    const auto exposure_compensation =
        constraints->exposureCompensation()->GetAsDouble();
    if (exposure_compensation < capabilities_->exposureCompensation()->min() ||
        exposure_compensation > capabilities_->exposureCompensation()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "exposureCompensation", "exposureCompensation setting out of range"));
      return;
    }
    temp_constraints->setExposureCompensation(
        constraints->exposureCompensation());
    settings->exposure_compensation = exposure_compensation;
  }

  settings->has_exposure_time =
      constraints->hasExposureTime() && constraints->exposureTime()->IsDouble();
  if (settings->has_exposure_time) {
    const auto exposure_time = constraints->exposureTime()->GetAsDouble();
    if (exposure_time < capabilities_->exposureTime()->min() ||
        exposure_time > capabilities_->exposureTime()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "exposureTime", "exposureTime setting out of range"));
      return;
    }
    temp_constraints->setExposureTime(constraints->exposureTime());
    settings->exposure_time = exposure_time;
  }
  settings->has_color_temperature = constraints->hasColorTemperature() &&
                                    constraints->colorTemperature()->IsDouble();
  if (settings->has_color_temperature) {
    const auto color_temperature =
        constraints->colorTemperature()->GetAsDouble();
    if (color_temperature < capabilities_->colorTemperature()->min() ||
        color_temperature > capabilities_->colorTemperature()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "colorTemperature", "colorTemperature setting out of range"));
      return;
    }
    temp_constraints->setColorTemperature(constraints->colorTemperature());
    settings->color_temperature = color_temperature;
  }
  settings->has_iso = constraints->hasIso() && constraints->iso()->IsDouble();
  if (settings->has_iso) {
    const auto iso = constraints->iso()->GetAsDouble();
    if (iso < capabilities_->iso()->min() ||
        iso > capabilities_->iso()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "iso", "iso setting out of range"));
      return;
    }
    temp_constraints->setIso(constraints->iso());
    settings->iso = iso;
  }

  settings->has_brightness =
      constraints->hasBrightness() && constraints->brightness()->IsDouble();
  if (settings->has_brightness) {
    const auto brightness = constraints->brightness()->GetAsDouble();
    if (brightness < capabilities_->brightness()->min() ||
        brightness > capabilities_->brightness()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "brightness", "brightness setting out of range"));
      return;
    }
    temp_constraints->setBrightness(constraints->brightness());
    settings->brightness = brightness;
  }
  settings->has_contrast =
      constraints->hasContrast() && constraints->contrast()->IsDouble();
  if (settings->has_contrast) {
    const auto contrast = constraints->contrast()->GetAsDouble();
    if (contrast < capabilities_->contrast()->min() ||
        contrast > capabilities_->contrast()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "contrast", "contrast setting out of range"));
      return;
    }
    temp_constraints->setContrast(constraints->contrast());
    settings->contrast = contrast;
  }
  settings->has_saturation =
      constraints->hasSaturation() && constraints->saturation()->IsDouble();
  if (settings->has_saturation) {
    const auto saturation = constraints->saturation()->GetAsDouble();
    if (saturation < capabilities_->saturation()->min() ||
        saturation > capabilities_->saturation()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "saturation", "saturation setting out of range"));
      return;
    }
    temp_constraints->setSaturation(constraints->saturation());
    settings->saturation = saturation;
  }
  settings->has_sharpness =
      constraints->hasSharpness() && constraints->sharpness()->IsDouble();
  if (settings->has_sharpness) {
    const auto sharpness = constraints->sharpness()->GetAsDouble();
    if (sharpness < capabilities_->sharpness()->min() ||
        sharpness > capabilities_->sharpness()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "sharpness", "sharpness setting out of range"));
      return;
    }
    temp_constraints->setSharpness(constraints->sharpness());
    settings->sharpness = sharpness;
  }

  settings->has_focus_distance = constraints->hasFocusDistance() &&
                                 constraints->focusDistance()->IsDouble();
  if (settings->has_focus_distance) {
    const auto focus_distance = constraints->focusDistance()->GetAsDouble();
    if (focus_distance < capabilities_->focusDistance()->min() ||
        focus_distance > capabilities_->focusDistance()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "focusDistance", "focusDistance setting out of range"));
      return;
    }
    temp_constraints->setFocusDistance(constraints->focusDistance());
    settings->focus_distance = focus_distance;
  }

  settings->has_pan = constraints->hasPan() && constraints->pan()->IsDouble();
  if (settings->has_pan) {
    if (!IsPageVisible()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "the page is not visible"));
      return;
    }
    const auto pan = constraints->pan()->GetAsDouble();
    if (pan < capabilities_->pan()->min() ||
        pan > capabilities_->pan()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "pan", "pan setting out of range"));
      return;
    }
    temp_constraints->setPan(constraints->pan());
    settings->pan = pan;
  }

  settings->has_tilt =
      constraints->hasTilt() && constraints->tilt()->IsDouble();
  if (settings->has_tilt) {
    if (!IsPageVisible()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "the page is not visible"));
      return;
    }
    const auto tilt = constraints->tilt()->GetAsDouble();
    if (tilt < capabilities_->tilt()->min() ||
        tilt > capabilities_->tilt()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "tilt", "tilt setting out of range"));
      return;
    }
    temp_constraints->setTilt(constraints->tilt());
    settings->tilt = tilt;
  }

  settings->has_zoom =
      constraints->hasZoom() && constraints->zoom()->IsDouble();
  if (settings->has_zoom) {
    if (!IsPageVisible()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "the page is not visible"));
      return;
    }
    const auto zoom = constraints->zoom()->GetAsDouble();
    if (zoom < capabilities_->zoom()->min() ||
        zoom > capabilities_->zoom()->max()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "zoom", "zoom setting out of range"));
      return;
    }
    temp_constraints->setZoom(constraints->zoom());
    settings->zoom = zoom;
  }

  // TODO(mcasas): support ConstrainBooleanParameters where applicable.
  settings->has_torch =
      constraints->hasTorch() && constraints->torch()->IsBoolean();
  if (settings->has_torch) {
    const auto torch = constraints->torch()->GetAsBoolean();
    if (torch && !capabilities_->torch()) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "torch", "torch not supported"));
      return;
    }
    temp_constraints->setTorch(constraints->torch());
    settings->torch = torch;
  }

  settings->has_background_blur_mode =
      constraints->hasBackgroundBlur() &&
      constraints->backgroundBlur()->IsBoolean();
  if (settings->has_background_blur_mode) {
    const auto background_blur = constraints->backgroundBlur()->GetAsBoolean();
    if (!base::Contains(capabilities_->backgroundBlur(), background_blur)) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          "backgroundBlur", "backgroundBlur setting value not supported"));
      return;
    }
    temp_constraints->setBackgroundBlur(constraints->backgroundBlur());
    settings->background_blur_mode =
        background_blur ? BackgroundBlurMode::BLUR : BackgroundBlurMode::OFF;
  }

  current_constraints_ = temp_constraints;

  service_requests_.insert(resolver);

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/false));
}

void ImageCapture::SetPanTiltZoomSettingsFromTrack(
    base::OnceClosure initialized_callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  auto* video_track = MediaStreamVideoTrack::From(stream_track_->Component());
  DCHECK(video_track);

  absl::optional<double> pan = video_track->pan();
  absl::optional<double> tilt = video_track->tilt();
  absl::optional<double> zoom = video_track->zoom();

  const bool ptz_requested =
      pan.has_value() || tilt.has_value() || zoom.has_value();
  const bool ptz_supported = capabilities_->hasPan() ||
                             capabilities_->hasTilt() ||
                             capabilities_->hasZoom();
  if (!ptz_supported || !ptz_requested || !HasPanTiltZoomPermissionGranted() ||
      !service_.is_bound()) {
    std::move(initialized_callback).Run();
    return;
  }

  ExecutionContext* context = GetExecutionContext();
  if (pan.has_value())
    UseCounter::Count(context, WebFeature::kImageCapturePan);
  if (tilt.has_value())
    UseCounter::Count(context, WebFeature::kImageCaptureTilt);
  if (zoom.has_value())
    UseCounter::Count(context, WebFeature::kImageCaptureZoom);

  auto settings = media::mojom::blink::PhotoSettings::New();

  if (capabilities_->hasPan() && pan.has_value() &&
      pan.value() >= capabilities_->pan()->min() &&
      pan.value() <= capabilities_->pan()->max()) {
    settings->has_pan = true;
    settings->pan = pan.value();
  }
  if (capabilities_->hasTilt() && tilt.has_value() &&
      tilt.value() >= capabilities_->tilt()->min() &&
      tilt.value() <= capabilities_->tilt()->max()) {
    settings->has_tilt = true;
    settings->tilt = tilt.value();
  }
  if (capabilities_->hasZoom() && zoom.has_value() &&
      zoom.value() >= capabilities_->zoom()->min() &&
      zoom.value() <= capabilities_->zoom()->max()) {
    settings->has_zoom = true;
    settings->zoom = zoom.value();
  }

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnSetPanTiltZoomSettingsFromTrack,
                    WrapPersistent(this), std::move(initialized_callback)));
}

void ImageCapture::OnSetPanTiltZoomSettingsFromTrack(
    base::OnceClosure done_callback,
    bool result) {
  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::UpdateMediaTrackSettingsAndCapabilities,
                    WrapPersistent(this), std::move(done_callback)));
}

const MediaTrackConstraintSet* ImageCapture::GetMediaTrackConstraints() const {
  return current_constraints_;
}

void ImageCapture::ClearMediaTrackConstraints() {
  current_constraints_ = nullptr;

  // TODO(mcasas): Clear also any PhotoSettings that the device might have got
  // configured, for that we need to know a "default" state of the device; take
  // a snapshot upon first opening. https://crbug.com/700607.
}

void ImageCapture::GetMediaTrackSettings(MediaTrackSettings* settings) const {
  // Merge any present |settings_| members into |settings|.
  CopySettings(settings_, settings,
               CopyPanTiltZoom(HasPanTiltZoomPermissionGranted()));
}

ImageCapture::ImageCapture(ExecutionContext* context,
                           MediaStreamTrack* track,
                           bool pan_tilt_zoom_allowed,
                           base::OnceClosure initialized_callback)
    : ExecutionContextLifecycleObserver(context),
      stream_track_(track),
      service_(context),
      pan_tilt_zoom_permission_(pan_tilt_zoom_allowed
                                    ? mojom::blink::PermissionStatus::GRANTED
                                    : mojom::blink::PermissionStatus::ASK),
      permission_service_(context),
      permission_observer_receiver_(this, context),
      capabilities_(MediaTrackCapabilities::Create()),
      settings_(MediaTrackSettings::Create()),
      photo_settings_(PhotoSettings::Create()) {
  DCHECK(stream_track_);
  DCHECK(!service_.is_bound());
  DCHECK(!permission_service_.is_bound());

  // This object may be constructed over an ExecutionContext that has already
  // been detached. In this case the ImageCapture service will not be available.
  if (!DomWindow())
    return;

  DomWindow()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kDOMManipulation)));

  service_.set_disconnect_handler(WTF::BindOnce(
      &ImageCapture::OnServiceConnectionError, WrapWeakPersistent(this)));

  // Launch a retrieval of the current photo state, which arrive asynchronously
  // to avoid blocking the main UI thread.
  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::SetPanTiltZoomSettingsFromTrack,
                    WrapPersistent(this), std::move(initialized_callback)));

  ConnectToPermissionService(
      context, permission_service_.BindNewPipeAndPassReceiver(
                   context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  permission_service_->AddPermissionObserver(
      CreateVideoCapturePermissionDescriptor(/*pan_tilt_zoom=*/true),
      pan_tilt_zoom_permission_, std::move(observer));
}

void ImageCapture::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  pan_tilt_zoom_permission_ = status;
}

bool ImageCapture::HasPanTiltZoomPermissionGranted() const {
  return pan_tilt_zoom_permission_ == mojom::blink::PermissionStatus::GRANTED;
}

ScriptPromise ImageCapture::GetMojoPhotoState(
    ScriptState* script_state,
    PromiseResolverFunction resolver_cb) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(resolver_cb),
                    /*trigger_take_photo=*/false));
  return promise;
}

void ImageCapture::OnMojoGetPhotoState(
    ScriptPromiseResolver* resolver,
    PromiseResolverFunction resolve_function,
    bool trigger_take_photo,
    media::mojom::blink::PhotoStatePtr photo_state) {
  DCHECK(service_requests_.Contains(resolver));

  if (photo_state.is_null()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
    service_requests_.erase(resolver);
    return;
  }

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, kInvalidStateTrackError));
    service_requests_.erase(resolver);
    return;
  }

  photo_settings_ = PhotoSettings::Create();
  photo_settings_->setImageHeight(photo_state->height->current);
  photo_settings_->setImageWidth(photo_state->width->current);
  // TODO(mcasas): collect the remaining two entries https://crbug.com/732521.

  photo_capabilities_ = MakeGarbageCollected<PhotoCapabilities>();
  photo_capabilities_->setRedEyeReduction(
      ToString(photo_state->red_eye_reduction));
  if (photo_state->height->min != 0 || photo_state->height->max != 0) {
    photo_capabilities_->setImageHeight(
        ToMediaSettingsRange(*photo_state->height));
  }
  if (photo_state->width->min != 0 || photo_state->width->max != 0) {
    photo_capabilities_->setImageWidth(
        ToMediaSettingsRange(*photo_state->width));
  }

  WTF::Vector<V8FillLightMode> fill_light_mode;
  for (const auto& mode : photo_state->fill_light_mode) {
    fill_light_mode.push_back(ToV8FillLightMode(mode));
  }
  if (!fill_light_mode.empty())
    photo_capabilities_->setFillLightMode(fill_light_mode);

  // Update the local track photo_state cache.
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  if (trigger_take_photo) {
    service_->TakePhoto(
        SourceId(),
        WTF::BindOnce(&ImageCapture::OnMojoTakePhoto, WrapPersistent(this),
                      WrapPersistent(resolver)));
    return;
  }

  std::move(resolve_function).Run(resolver);
  service_requests_.erase(resolver);
}

void ImageCapture::OnMojoSetPhotoOptions(ScriptPromiseResolver* resolver,
                                         bool trigger_take_photo,
                                         bool result) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoSetPhotoOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  if (!result) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "setPhotoOptions failed"));
    service_requests_.erase(resolver);
    return;
  }

  auto resolver_cb =
      WTF::BindOnce(&ImageCapture::ResolveWithNothing, WrapPersistent(this));

  // Retrieve the current device status after setting the options.
  service_->GetPhotoState(
      SourceId(), WTF::BindOnce(&ImageCapture::OnMojoGetPhotoState,
                                WrapPersistent(this), WrapPersistent(resolver),
                                std::move(resolver_cb), trigger_take_photo));
}

void ImageCapture::OnMojoTakePhoto(ScriptPromiseResolver* resolver,
                                   media::mojom::blink::BlobPtr blob) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoTakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.empty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
  } else {
    resolver->Resolve(
        Blob::Create(blob->data.data(), blob->data.size(), blob->mime_type));
  }
  service_requests_.erase(resolver);
}

void ImageCapture::UpdateMediaTrackSettingsAndCapabilities(
    base::OnceClosure initialized_callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  if (!photo_state) {
    std::move(initialized_callback).Run();
    return;
  }

  WTF::Vector<WTF::String> supported_white_balance_modes;
  supported_white_balance_modes.ReserveInitialCapacity(
      photo_state->supported_white_balance_modes.size());
  for (const auto& supported_mode : photo_state->supported_white_balance_modes)
    supported_white_balance_modes.push_back(ToString(supported_mode));
  if (!supported_white_balance_modes.empty()) {
    capabilities_->setWhiteBalanceMode(
        std::move(supported_white_balance_modes));
    settings_->setWhiteBalanceMode(
        ToString(photo_state->current_white_balance_mode));
  }

  WTF::Vector<WTF::String> supported_exposure_modes;
  supported_exposure_modes.ReserveInitialCapacity(
      photo_state->supported_exposure_modes.size());
  for (const auto& supported_mode : photo_state->supported_exposure_modes)
    supported_exposure_modes.push_back(ToString(supported_mode));
  if (!supported_exposure_modes.empty()) {
    capabilities_->setExposureMode(std::move(supported_exposure_modes));
    settings_->setExposureMode(ToString(photo_state->current_exposure_mode));
  }

  WTF::Vector<WTF::String> supported_focus_modes;
  supported_focus_modes.ReserveInitialCapacity(
      photo_state->supported_focus_modes.size());
  for (const auto& supported_mode : photo_state->supported_focus_modes)
    supported_focus_modes.push_back(ToString(supported_mode));
  if (!supported_focus_modes.empty()) {
    capabilities_->setFocusMode(std::move(supported_focus_modes));
    settings_->setFocusMode(ToString(photo_state->current_focus_mode));
  }

  HeapVector<Member<Point2D>> current_points_of_interest;
  if (!photo_state->points_of_interest.empty()) {
    for (const auto& point : photo_state->points_of_interest) {
      Point2D* web_point = Point2D::Create();
      web_point->setX(point->x);
      web_point->setY(point->y);
      current_points_of_interest.push_back(web_point);
    }
  }
  settings_->setPointsOfInterest(current_points_of_interest);

  if (photo_state->exposure_compensation->max !=
      photo_state->exposure_compensation->min) {
    capabilities_->setExposureCompensation(
        ToMediaSettingsRange(*photo_state->exposure_compensation));
    settings_->setExposureCompensation(
        photo_state->exposure_compensation->current);
  }
  if (photo_state->exposure_time->max != photo_state->exposure_time->min) {
    capabilities_->setExposureTime(
        ToMediaSettingsRange(*photo_state->exposure_time));
    settings_->setExposureTime(photo_state->exposure_time->current);
  }
  if (photo_state->color_temperature->max !=
      photo_state->color_temperature->min) {
    capabilities_->setColorTemperature(
        ToMediaSettingsRange(*photo_state->color_temperature));
    settings_->setColorTemperature(photo_state->color_temperature->current);
  }
  if (photo_state->iso->max != photo_state->iso->min) {
    capabilities_->setIso(ToMediaSettingsRange(*photo_state->iso));
    settings_->setIso(photo_state->iso->current);
  }

  if (photo_state->brightness->max != photo_state->brightness->min) {
    capabilities_->setBrightness(
        ToMediaSettingsRange(*photo_state->brightness));
    settings_->setBrightness(photo_state->brightness->current);
  }
  if (photo_state->contrast->max != photo_state->contrast->min) {
    capabilities_->setContrast(ToMediaSettingsRange(*photo_state->contrast));
    settings_->setContrast(photo_state->contrast->current);
  }
  if (photo_state->saturation->max != photo_state->saturation->min) {
    capabilities_->setSaturation(
        ToMediaSettingsRange(*photo_state->saturation));
    settings_->setSaturation(photo_state->saturation->current);
  }
  if (photo_state->sharpness->max != photo_state->sharpness->min) {
    capabilities_->setSharpness(ToMediaSettingsRange(*photo_state->sharpness));
    settings_->setSharpness(photo_state->sharpness->current);
  }

  if (photo_state->focus_distance->max != photo_state->focus_distance->min) {
    capabilities_->setFocusDistance(
        ToMediaSettingsRange(*photo_state->focus_distance));
    settings_->setFocusDistance(photo_state->focus_distance->current);
  }

  if (HasPanTiltZoomPermissionGranted()) {
    if (photo_state->pan->max != photo_state->pan->min) {
      capabilities_->setPan(ToMediaSettingsRange(*photo_state->pan));
      settings_->setPan(photo_state->pan->current);
    }
    if (photo_state->tilt->max != photo_state->tilt->min) {
      capabilities_->setTilt(ToMediaSettingsRange(*photo_state->tilt));
      settings_->setTilt(photo_state->tilt->current);
    }
    if (photo_state->zoom->max != photo_state->zoom->min) {
      capabilities_->setZoom(ToMediaSettingsRange(*photo_state->zoom));
      settings_->setZoom(photo_state->zoom->current);
    }
  }

  if (photo_state->supports_torch)
    capabilities_->setTorch(photo_state->supports_torch);
  if (photo_state->supports_torch)
    settings_->setTorch(photo_state->torch);

  if (photo_state->supported_background_blur_modes &&
      !photo_state->supported_background_blur_modes->empty()) {
    Vector<bool> supported_background_blur_modes;
    for (auto mode : *photo_state->supported_background_blur_modes)
      supported_background_blur_modes.push_back(ToBooleanMode(mode));
    capabilities_->setBackgroundBlur(
        std::move(supported_background_blur_modes));
    settings_->setBackgroundBlur(
        ToBooleanMode(photo_state->background_blur_mode));
  }

  std::move(initialized_callback).Run();
}

void ImageCapture::OnServiceConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolver>> resolvers;
  resolvers.swap(service_requests_);
  for (ScriptPromiseResolver* resolver : resolvers) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
  }
}

void ImageCapture::ResolveWithNothing(ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve();
}

void ImageCapture::ResolveWithPhotoSettings(ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve(photo_settings_);
}

void ImageCapture::ResolveWithPhotoCapabilities(
    ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve(photo_capabilities_);
}

bool ImageCapture::IsPageVisible() {
  return DomWindow() ? DomWindow()->document()->IsPageVisible() : false;
}

const String& ImageCapture::SourceId() const {
  return stream_track_->Component()->Source()->Id();
}

ImageCapture* ImageCapture::Clone() const {
  ImageCapture* clone = MakeGarbageCollected<ImageCapture>(
      GetExecutionContext(), stream_track_, HasPanTiltZoomPermissionGranted(),
      /*callback=*/base::DoNothing());

  // Copy capabilities.
  CopyCapabilities(capabilities_, clone->capabilities_, CopyPanTiltZoom(true));

  // Copy settings.
  CopySettings(settings_, clone->settings_, CopyPanTiltZoom(true));

  // Copy current constraints.
  if (current_constraints_) {
    clone->current_constraints_ = MediaTrackConstraintSet::Create();
    CopyConstraintSet(current_constraints_, clone->current_constraints_,
                      CopyPanTiltZoom(true));
  }

  return clone;
}

void ImageCapture::Trace(Visitor* visitor) const {
  visitor->Trace(stream_track_);
  visitor->Trace(service_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receiver_);
  visitor->Trace(capabilities_);
  visitor->Trace(settings_);
  visitor->Trace(photo_settings_);
  visitor->Trace(current_constraints_);
  visitor->Trace(photo_capabilities_);
  visitor->Trace(service_requests_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
