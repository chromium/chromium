// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture_frame_grabber.h"
#include "third_party/blink/renderer/modules/imagecapture/media_settings_range.h"
#include "third_party/blink/renderer/modules/imagecapture/photo_capabilities.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_capabilities.h"
#include "third_party/blink/renderer/modules/mediastream/media_track_constraints.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using FillLightMode = media::mojom::blink::FillLightMode;
using MeteringMode = media::mojom::blink::MeteringMode;

namespace {

const char kNoServiceError[] = "ImageCapture service unavailable.";

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
    default:
      NOTREACHED() << "Unknown MeteringMode";
  }
  return WebString();
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

  return MakeGarbageCollected<ImageCapture>(context, track);
}

ImageCapture::~ImageCapture() {
  DCHECK(!HasEventListeners());
  // There should be no more outstanding |m_serviceRequests| at this point
  // since each of them holds a persistent handle to this object.
  DCHECK(service_requests_.IsEmpty());
}

const AtomicString& ImageCapture::InterfaceName() const {
  return event_target_names::kImageCapture;
}

ExecutionContext* ImageCapture::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool ImageCapture::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void ImageCapture::ContextDestroyed(ExecutionContext*) {
  RemoveAllEventListeners();
  service_requests_.clear();
  DCHECK(!HasEventListeners());
}

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!service_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  auto resolver_cb = WTF::Bind(&ImageCapture::ResolveWithPhotoCapabilities,
                               WrapPersistent(this));

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  service_->GetPhotoState(
      stream_track_->Component()->Source()->Id(),
      WTF::Bind(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                WrapPersistent(resolver), WTF::Passed(std::move(resolver_cb)),
                false /* trigger_take_photo */));
  return promise;
}

ScriptPromise ImageCapture::getPhotoSettings(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!service_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  auto resolver_cb =
      WTF::Bind(&ImageCapture::ResolveWithPhotoSettings, WrapPersistent(this));

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  service_->GetPhotoState(
      stream_track_->Component()->Source()->Id(),
      WTF::Bind(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                WrapPersistent(resolver), WTF::Passed(std::move(resolver_cb)),
                false /* trigger_take_photo */));
  return promise;
}

ScriptPromise ImageCapture::setOptions(ScriptState* script_state,
                                       const PhotoSettings* photo_settings,
                                       bool trigger_take_photo /* = false */) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::setOptions", TRACE_EVENT_SCOPE_PROCESS);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The associated Track is in an invalid state."));
    return promise;
  }

  if (!service_) {
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
    if (photo_capabilities_ &&
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
    if (photo_capabilities_ &&
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
    if (photo_capabilities_ &&
        !photo_capabilities_->IsRedEyeReductionControllable()) {
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
    if (photo_capabilities_ && photo_capabilities_->fillLightMode().Find(
                                   fill_light_mode) == kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported fillLightMode"));
      return promise;
    }
    settings->fill_light_mode = ParseFillLightMode(fill_light_mode);
  }

  service_->SetOptions(
      stream_track_->Component()->Source()->Id(), std::move(settings),
      WTF::Bind(&ImageCapture::OnMojoSetOptions, WrapPersistent(this),
                WrapPersistent(resolver), trigger_take_photo));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::takePhoto", TRACE_EVENT_SCOPE_PROCESS);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The associated Track is in an invalid state."));
    return promise;
  }
  if (!service_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }

  service_requests_.insert(resolver);

  // m_streamTrack->component()->source()->id() is the renderer "name" of the
  // camera;
  // TODO(mcasas) consider sending the security origin as well:
  // scriptState->getExecutionContext()->getSecurityOrigin()->toString()
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::takePhoto", TRACE_EVENT_SCOPE_PROCESS);
  service_->TakePhoto(
      stream_track_->Component()->Source()->Id(),
      WTF::Bind(&ImageCapture::OnMojoTakePhoto, WrapPersistent(this),
                WrapPersistent(resolver)));
  return promise;
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state,
                                      const PhotoSettings* photo_settings) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::takePhoto (with settings)",
                       TRACE_EVENT_SCOPE_PROCESS);

  return setOptions(script_state, photo_settings,
                    true /* trigger_take_photo */);
}

ScriptPromise ImageCapture::grabFrame(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The associated Track is in an invalid state."));
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

  // The platform does not know about MediaStreamTrack, so we wrap it up.
  WebMediaStreamTrack track(stream_track_->Component());
  auto resolver_callback_adapter =
      std::make_unique<CallbackPromiseAdapter<ImageBitmap, void>>(resolver);
  frame_grabber_->GrabFrame(&track, std::move(resolver_callback_adapter),
                            ExecutionContext::From(script_state)
                                ->GetTaskRunner(TaskType::kDOMManipulation));

  return promise;
}

MediaTrackCapabilities* ImageCapture::GetMediaTrackCapabilities() const {
  return capabilities_;
}

// TODO(mcasas): make the implementation fully Spec compliant, see the TODOs
// inside the method, https://crbug.com/708723.
void ImageCapture::SetMediaTrackConstraints(
    ScriptPromiseResolver* resolver,
    const HeapVector<Member<MediaTrackConstraintSet>>& constraints_vector) {
  DCHECK_GT(constraints_vector.size(), 0u);
  if (!service_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return;
  }
  // TODO(mcasas): add support more than one single advanced constraint.
  const MediaTrackConstraintSet* constraints = constraints_vector[0];

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
      (constraints->hasPan() && !capabilities_->hasPan()) ||
      (constraints->hasTilt() && !capabilities_->hasTilt()) ||
      (constraints->hasZoom() && !capabilities_->hasZoom()) ||
      (constraints->hasTorch() && !capabilities_->hasTorch())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, "Unsupported constraint(s)"));
    return;
  }

  auto settings = media::mojom::blink::PhotoSettings::New();
  MediaTrackConstraintSet* temp_constraints = current_constraints_;

  // TODO(mcasas): support other Mode types beyond simple string i.e. the
  // equivalents of "sequence<DOMString>"" or "ConstrainDOMStringParameters".
  settings->has_white_balance_mode = constraints->hasWhiteBalanceMode() &&
                                     constraints->whiteBalanceMode().IsString();
  if (settings->has_white_balance_mode) {
    const auto white_balance_mode =
        constraints->whiteBalanceMode().GetAsString();
    if (capabilities_->whiteBalanceMode().Find(white_balance_mode) ==
        kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Unsupported whiteBalanceMode."));
      return;
    }
    temp_constraints->setWhiteBalanceMode(constraints->whiteBalanceMode());
    settings->white_balance_mode = ParseMeteringMode(white_balance_mode);
  }
  settings->has_exposure_mode =
      constraints->hasExposureMode() && constraints->exposureMode().IsString();
  if (settings->has_exposure_mode) {
    const auto exposure_mode = constraints->exposureMode().GetAsString();
    if (capabilities_->exposureMode().Find(exposure_mode) == kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported exposureMode."));
      return;
    }
    temp_constraints->setExposureMode(constraints->exposureMode());
    settings->exposure_mode = ParseMeteringMode(exposure_mode);
  }

  settings->has_focus_mode =
      constraints->hasFocusMode() && constraints->focusMode().IsString();
  if (settings->has_focus_mode) {
    const auto focus_mode = constraints->focusMode().GetAsString();
    if (capabilities_->focusMode().Find(focus_mode) == kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported focusMode."));
      return;
    }
    temp_constraints->setFocusMode(constraints->focusMode());
    settings->focus_mode = ParseMeteringMode(focus_mode);
  }

  // TODO(mcasas): support ConstrainPoint2DParameters.
  if (constraints->hasPointsOfInterest() &&
      constraints->pointsOfInterest().IsPoint2DSequence()) {
    for (const auto& point :
         constraints->pointsOfInterest().GetAsPoint2DSequence()) {
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
      constraints->exposureCompensation().IsDouble();
  if (settings->has_exposure_compensation) {
    const auto exposure_compensation =
        constraints->exposureCompensation().GetAsDouble();
    if (exposure_compensation < capabilities_->exposureCompensation()->min() ||
        exposure_compensation > capabilities_->exposureCompensation()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "exposureCompensation setting out of range"));
      return;
    }
    temp_constraints->setExposureCompensation(
        constraints->exposureCompensation());
    settings->exposure_compensation = exposure_compensation;
  }

  settings->has_exposure_time =
      constraints->hasExposureTime() && constraints->exposureTime().IsDouble();
  if (settings->has_exposure_time) {
    const auto exposure_time = constraints->exposureTime().GetAsDouble();
    if (exposure_time < capabilities_->exposureTime()->min() ||
        exposure_time > capabilities_->exposureTime()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "exposureTime setting out of range"));
      return;
    }
    temp_constraints->setExposureTime(constraints->exposureTime());
    settings->exposure_time = exposure_time;
  }
  settings->has_color_temperature = constraints->hasColorTemperature() &&
                                    constraints->colorTemperature().IsDouble();
  if (settings->has_color_temperature) {
    const auto color_temperature =
        constraints->colorTemperature().GetAsDouble();
    if (color_temperature < capabilities_->colorTemperature()->min() ||
        color_temperature > capabilities_->colorTemperature()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "colorTemperature setting out of range"));
      return;
    }
    temp_constraints->setColorTemperature(constraints->colorTemperature());
    settings->color_temperature = color_temperature;
  }
  settings->has_iso = constraints->hasIso() && constraints->iso().IsDouble();
  if (settings->has_iso) {
    const auto iso = constraints->iso().GetAsDouble();
    if (iso < capabilities_->iso()->min() ||
        iso > capabilities_->iso()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "iso setting out of range"));
      return;
    }
    temp_constraints->setIso(constraints->iso());
    settings->iso = iso;
  }

  settings->has_brightness =
      constraints->hasBrightness() && constraints->brightness().IsDouble();
  if (settings->has_brightness) {
    const auto brightness = constraints->brightness().GetAsDouble();
    if (brightness < capabilities_->brightness()->min() ||
        brightness > capabilities_->brightness()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "brightness setting out of range"));
      return;
    }
    temp_constraints->setBrightness(constraints->brightness());
    settings->brightness = brightness;
  }
  settings->has_contrast =
      constraints->hasContrast() && constraints->contrast().IsDouble();
  if (settings->has_contrast) {
    const auto contrast = constraints->contrast().GetAsDouble();
    if (contrast < capabilities_->contrast()->min() ||
        contrast > capabilities_->contrast()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "contrast setting out of range"));
      return;
    }
    temp_constraints->setContrast(constraints->contrast());
    settings->contrast = contrast;
  }
  settings->has_saturation =
      constraints->hasSaturation() && constraints->saturation().IsDouble();
  if (settings->has_saturation) {
    const auto saturation = constraints->saturation().GetAsDouble();
    if (saturation < capabilities_->saturation()->min() ||
        saturation > capabilities_->saturation()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "saturation setting out of range"));
      return;
    }
    temp_constraints->setSaturation(constraints->saturation());
    settings->saturation = saturation;
  }
  settings->has_sharpness =
      constraints->hasSharpness() && constraints->sharpness().IsDouble();
  if (settings->has_sharpness) {
    const auto sharpness = constraints->sharpness().GetAsDouble();
    if (sharpness < capabilities_->sharpness()->min() ||
        sharpness > capabilities_->sharpness()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "sharpness setting out of range"));
      return;
    }
    temp_constraints->setSharpness(constraints->sharpness());
    settings->sharpness = sharpness;
  }

  settings->has_focus_distance = constraints->hasFocusDistance() &&
                                 constraints->focusDistance().IsDouble();
  if (settings->has_focus_distance) {
    const auto focus_distance = constraints->focusDistance().GetAsDouble();
    if (focus_distance < capabilities_->focusDistance()->min() ||
        focus_distance > capabilities_->focusDistance()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "focusDistance setting out of range"));
      return;
    }
    temp_constraints->setFocusDistance(constraints->focusDistance());
    settings->focus_distance = focus_distance;
  }

  settings->has_pan = constraints->hasPan() && constraints->pan().IsDouble();
  if (settings->has_pan) {
    const auto pan = constraints->pan().GetAsDouble();
    if (pan < capabilities_->pan()->min() ||
        pan > capabilities_->pan()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "pan setting out of range"));
      return;
    }
    temp_constraints->setPan(constraints->pan());
    settings->pan = pan;
  }

  settings->has_tilt = constraints->hasTilt() && constraints->tilt().IsDouble();
  if (settings->has_tilt) {
    const auto tilt = constraints->tilt().GetAsDouble();
    if (tilt < capabilities_->tilt()->min() ||
        tilt > capabilities_->tilt()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "tilt setting out of range"));
      return;
    }
    temp_constraints->setTilt(constraints->tilt());
    settings->tilt = tilt;
  }

  settings->has_zoom = constraints->hasZoom() && constraints->zoom().IsDouble();
  if (settings->has_zoom) {
    const auto zoom = constraints->zoom().GetAsDouble();
    if (zoom < capabilities_->zoom()->min() ||
        zoom > capabilities_->zoom()->max()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "zoom setting out of range"));
      return;
    }
    temp_constraints->setZoom(constraints->zoom());
    settings->zoom = zoom;
  }

  // TODO(mcasas): support ConstrainBooleanParameters where applicable.
  settings->has_torch =
      constraints->hasTorch() && constraints->torch().IsBoolean();
  if (settings->has_torch) {
    const auto torch = constraints->torch().GetAsBoolean();
    if (torch && !capabilities_->torch()) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "torch not supported"));
      return;
    }
    temp_constraints->setTorch(constraints->torch());
    settings->torch = torch;
  }

  current_constraints_ = temp_constraints;

  service_requests_.insert(resolver);

  service_->SetOptions(
      stream_track_->Component()->Source()->Id(), std::move(settings),
      WTF::Bind(&ImageCapture::OnMojoSetOptions, WrapPersistent(this),
                WrapPersistent(resolver), false /* trigger_take_photo */));
}

const MediaTrackConstraintSet* ImageCapture::GetMediaTrackConstraints() const {
  return current_constraints_;
}

void ImageCapture::ClearMediaTrackConstraints() {
  current_constraints_ = MediaTrackConstraintSet::Create();

  // TODO(mcasas): Clear also any PhotoSettings that the device might have got
  // configured, for that we need to know a "default" state of the device; take
  // a snapshot upon first opening. https://crbug.com/700607.
}

void ImageCapture::GetMediaTrackSettings(MediaTrackSettings* settings) const {
  // Merge any present |settings_| members into |settings|.

  if (settings_->hasWhiteBalanceMode())
    settings->setWhiteBalanceMode(settings_->whiteBalanceMode());
  if (settings_->hasExposureMode())
    settings->setExposureMode(settings_->exposureMode());
  if (settings_->hasFocusMode())
    settings->setFocusMode(settings_->focusMode());

  if (settings_->hasPointsOfInterest() &&
      !settings_->pointsOfInterest().IsEmpty()) {
    settings->setPointsOfInterest(settings_->pointsOfInterest());
  }

  if (settings_->hasExposureCompensation())
    settings->setExposureCompensation(settings_->exposureCompensation());
  if (settings_->hasExposureTime())
    settings->setExposureTime(settings_->exposureTime());
  if (settings_->hasColorTemperature())
    settings->setColorTemperature(settings_->colorTemperature());
  if (settings_->hasIso())
    settings->setIso(settings_->iso());

  if (settings_->hasBrightness())
    settings->setBrightness(settings_->brightness());
  if (settings_->hasContrast())
    settings->setContrast(settings_->contrast());
  if (settings_->hasSaturation())
    settings->setSaturation(settings_->saturation());
  if (settings_->hasSharpness())
    settings->setSharpness(settings_->sharpness());

  if (settings_->hasFocusDistance())
    settings->setFocusDistance(settings_->focusDistance());

  if (settings_->hasPan())
    settings->setPan(settings_->pan());
  if (settings_->hasTilt())
    settings->setTilt(settings_->tilt());
  if (settings_->hasZoom())
    settings->setZoom(settings_->zoom());
  if (settings_->hasTorch())
    settings->setTorch(settings_->torch());
}

ImageCapture::ImageCapture(ExecutionContext* context, MediaStreamTrack* track)
    : ContextLifecycleObserver(context),
      stream_track_(track),
      capabilities_(MediaTrackCapabilities::Create()),
      settings_(MediaTrackSettings::Create()),
      current_constraints_(MediaTrackConstraintSet::Create()),
      photo_settings_(PhotoSettings::Create()) {
  DCHECK(stream_track_);
  DCHECK(!service_.is_bound());

  // This object may be constructed over an ExecutionContext that has already
  // been detached. In this case the ImageCapture service will not be available.
  if (!GetFrame())
    return;

  GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver());

  service_.set_disconnect_handler(WTF::Bind(
      &ImageCapture::OnServiceConnectionError, WrapWeakPersistent(this)));

  // Launch a retrieval of the current photo state, which arrive asynchronously
  // to avoid blocking the main UI thread.
  service_->GetPhotoState(stream_track_->Component()->Source()->Id(),
                          WTF::Bind(&ImageCapture::UpdateMediaTrackCapabilities,
                                    WrapPersistent(this)));
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

  photo_settings_ = PhotoSettings::Create();
  photo_settings_->setImageHeight(photo_state->height->current);
  photo_settings_->setImageWidth(photo_state->width->current);
  // TODO(mcasas): collect the remaining two entries https://crbug.com/732521.

  photo_capabilities_ = MakeGarbageCollected<PhotoCapabilities>();
  photo_capabilities_->SetRedEyeReduction(photo_state->red_eye_reduction);
  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (photo_state->height->min != 0 || photo_state->height->max != 0) {
    photo_capabilities_->SetImageHeight(
        MediaSettingsRange::Create(std::move(photo_state->height)));
  }
  if (photo_state->width->min != 0 || photo_state->width->max != 0) {
    photo_capabilities_->SetImageWidth(
        MediaSettingsRange::Create(std::move(photo_state->width)));
  }
  if (!photo_state->fill_light_mode.IsEmpty())
    photo_capabilities_->SetFillLightMode(photo_state->fill_light_mode);

  // Update the local track photo_state cache.
  UpdateMediaTrackCapabilities(std::move(photo_state));

  if (trigger_take_photo) {
    service_->TakePhoto(
        stream_track_->Component()->Source()->Id(),
        WTF::Bind(&ImageCapture::OnMojoTakePhoto, WrapPersistent(this),
                  WrapPersistent(resolver)));
    return;
  }

  std::move(resolve_function).Run(resolver);
  service_requests_.erase(resolver);
}

void ImageCapture::OnMojoSetOptions(ScriptPromiseResolver* resolver,
                                    bool trigger_take_photo,
                                    bool result) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoSetOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  if (!result) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "setOptions failed"));
    service_requests_.erase(resolver);
    return;
  }

  auto resolver_cb =
      WTF::Bind(&ImageCapture::ResolveWithNothing, WrapPersistent(this));

  // Retrieve the current device status after setting the options.
  service_->GetPhotoState(
      stream_track_->Component()->Source()->Id(),
      WTF::Bind(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                WrapPersistent(resolver), WTF::Passed(std::move(resolver_cb)),
                trigger_take_photo));
}

void ImageCapture::OnMojoTakePhoto(ScriptPromiseResolver* resolver,
                                   media::mojom::blink::BlobPtr blob) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoTakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.IsEmpty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
  } else {
    resolver->Resolve(
        Blob::Create(blob->data.data(), blob->data.size(), blob->mime_type));
  }
  service_requests_.erase(resolver);
}

void ImageCapture::UpdateMediaTrackCapabilities(
    media::mojom::blink::PhotoStatePtr photo_state) {
  if (!photo_state)
    return;

  WTF::Vector<WTF::String> supported_white_balance_modes;
  supported_white_balance_modes.ReserveInitialCapacity(
      photo_state->supported_white_balance_modes.size());
  for (const auto& supported_mode : photo_state->supported_white_balance_modes)
    supported_white_balance_modes.push_back(ToString(supported_mode));
  if (!supported_white_balance_modes.IsEmpty()) {
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
  if (!supported_exposure_modes.IsEmpty()) {
    capabilities_->setExposureMode(std::move(supported_exposure_modes));
    settings_->setExposureMode(ToString(photo_state->current_exposure_mode));
  }

  WTF::Vector<WTF::String> supported_focus_modes;
  supported_focus_modes.ReserveInitialCapacity(
      photo_state->supported_focus_modes.size());
  for (const auto& supported_mode : photo_state->supported_focus_modes)
    supported_focus_modes.push_back(ToString(supported_mode));
  if (!supported_focus_modes.IsEmpty()) {
    capabilities_->setFocusMode(std::move(supported_focus_modes));
    settings_->setFocusMode(ToString(photo_state->current_focus_mode));
  }

  HeapVector<Member<Point2D>> current_points_of_interest;
  if (!photo_state->points_of_interest.IsEmpty()) {
    for (const auto& point : photo_state->points_of_interest) {
      Point2D* web_point = Point2D::Create();
      web_point->setX(point->x);
      web_point->setY(point->y);
      current_points_of_interest.push_back(web_point);
    }
  }
  settings_->setPointsOfInterest(current_points_of_interest);

  // TODO(mcasas): Remove the explicit MediaSettingsRange::create() when
  // mojo::StructTraits supports garbage-collected mappings,
  // https://crbug.com/700180.
  if (photo_state->exposure_compensation->max !=
      photo_state->exposure_compensation->min) {
    capabilities_->setExposureCompensation(
        MediaSettingsRange::Create(*photo_state->exposure_compensation));
    settings_->setExposureCompensation(
        photo_state->exposure_compensation->current);
  }
  if (photo_state->exposure_time->max != photo_state->exposure_time->min) {
    capabilities_->setExposureTime(
        MediaSettingsRange::Create(*photo_state->exposure_time));
    settings_->setExposureTime(photo_state->exposure_time->current);
  }
  if (photo_state->color_temperature->max !=
      photo_state->color_temperature->min) {
    capabilities_->setColorTemperature(
        MediaSettingsRange::Create(*photo_state->color_temperature));
    settings_->setColorTemperature(photo_state->color_temperature->current);
  }
  if (photo_state->iso->max != photo_state->iso->min) {
    capabilities_->setIso(MediaSettingsRange::Create(*photo_state->iso));
    settings_->setIso(photo_state->iso->current);
  }

  if (photo_state->brightness->max != photo_state->brightness->min) {
    capabilities_->setBrightness(
        MediaSettingsRange::Create(*photo_state->brightness));
    settings_->setBrightness(photo_state->brightness->current);
  }
  if (photo_state->contrast->max != photo_state->contrast->min) {
    capabilities_->setContrast(
        MediaSettingsRange::Create(*photo_state->contrast));
    settings_->setContrast(photo_state->contrast->current);
  }
  if (photo_state->saturation->max != photo_state->saturation->min) {
    capabilities_->setSaturation(
        MediaSettingsRange::Create(*photo_state->saturation));
    settings_->setSaturation(photo_state->saturation->current);
  }
  if (photo_state->sharpness->max != photo_state->sharpness->min) {
    capabilities_->setSharpness(
        MediaSettingsRange::Create(*photo_state->sharpness));
    settings_->setSharpness(photo_state->sharpness->current);
  }

  if (photo_state->focus_distance->max != photo_state->focus_distance->min) {
    capabilities_->setFocusDistance(
        MediaSettingsRange::Create(*photo_state->focus_distance));
    settings_->setFocusDistance(photo_state->focus_distance->current);
  }

  if (photo_state->pan->max != photo_state->pan->min) {
    capabilities_->setPan(MediaSettingsRange::Create(*photo_state->pan));
    settings_->setPan(photo_state->pan->current);
  }
  if (photo_state->tilt->max != photo_state->tilt->min) {
    capabilities_->setTilt(MediaSettingsRange::Create(*photo_state->tilt));
    settings_->setTilt(photo_state->tilt->current);
  }
  if (photo_state->zoom->max != photo_state->zoom->min) {
    capabilities_->setZoom(MediaSettingsRange::Create(*photo_state->zoom));
    settings_->setZoom(photo_state->zoom->current);
  }

  if (photo_state->supports_torch)
    capabilities_->setTorch(photo_state->supports_torch);
  if (photo_state->supports_torch)
    settings_->setTorch(photo_state->torch);
}

void ImageCapture::OnServiceConnectionError() {
  service_.reset();
  for (ScriptPromiseResolver* resolver : service_requests_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
  }
  service_requests_.clear();
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

void ImageCapture::Trace(blink::Visitor* visitor) {
  visitor->Trace(stream_track_);
  visitor->Trace(capabilities_);
  visitor->Trace(settings_);
  visitor->Trace(photo_settings_);
  visitor->Trace(current_constraints_);
  visitor->Trace(photo_capabilities_);
  visitor->Trace(service_requests_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
