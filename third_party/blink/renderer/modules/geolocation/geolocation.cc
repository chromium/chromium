/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/geolocation/geolocation.h"

#include "services/device/public/mojom/geoposition.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_coordinates.h"
#include "third_party/blink/renderer/modules/geolocation/geolocation_error.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {
namespace {

const char kPermissionDeniedErrorMessage[] = "User denied Geolocation";
const char kFeaturePolicyErrorMessage[] =
    "Geolocation has been disabled in this document by Feature Policy.";
const char kFeaturePolicyConsoleWarning[] =
    "Geolocation access has been blocked because of a Feature Policy applied "
    "to the current document. See https://goo.gl/EuHzyv for more details.";

Geoposition* CreateGeoposition(
    const device::mojom::blink::Geoposition& position) {
  auto* coordinates = MakeGarbageCollected<GeolocationCoordinates>(
      position.latitude, position.longitude,
      // Lowest point on land is at approximately -400 meters.
      position.altitude > -10000., position.altitude, position.accuracy,
      position.altitude_accuracy >= 0., position.altitude_accuracy,
      position.heading >= 0. && position.heading <= 360., position.heading,
      position.speed >= 0., position.speed);
  return MakeGarbageCollected<Geoposition>(
      coordinates,
      ConvertSecondsToDOMTimeStamp(position.timestamp.ToDoubleT()));
}

GeolocationPositionError* CreatePositionError(
    device::mojom::blink::Geoposition::ErrorCode mojom_error_code,
    const String& error) {
  GeolocationPositionError::ErrorCode error_code =
      GeolocationPositionError::kPositionUnavailable;
  switch (mojom_error_code) {
    case device::mojom::blink::Geoposition::ErrorCode::PERMISSION_DENIED:
      error_code = GeolocationPositionError::kPermissionDenied;
      break;
    case device::mojom::blink::Geoposition::ErrorCode::POSITION_UNAVAILABLE:
      error_code = GeolocationPositionError::kPositionUnavailable;
      break;
    case device::mojom::blink::Geoposition::ErrorCode::NONE:
    case device::mojom::blink::Geoposition::ErrorCode::TIMEOUT:
      NOTREACHED();
      break;
  }
  return MakeGarbageCollected<GeolocationPositionError>(error_code, error);
}

static void ReportGeolocationViolation(Document* doc) {
  // TODO(dcheng): |doc| probably can't be null here.
  if (!LocalFrame::HasTransientUserActivation(doc ? doc->GetFrame()
                                                  : nullptr)) {
    PerformanceMonitor::ReportGenericViolation(
        doc, PerformanceMonitor::kDiscouragedAPIUse,
        "Only request geolocation information in response to a user gesture.",
        base::TimeDelta(), nullptr);
  }
}

}  // namespace

Geolocation* Geolocation::Create(ExecutionContext* context) {
  return MakeGarbageCollected<Geolocation>(context);
}

Geolocation::Geolocation(ExecutionContext* context)
    : ContextLifecycleObserver(context),
      PageVisibilityObserver(GetDocument()->GetPage()),
      watchers_(MakeGarbageCollected<GeolocationWatchers>()) {}

Geolocation::~Geolocation() = default;

void Geolocation::Trace(blink::Visitor* visitor) {
  visitor->Trace(one_shots_);
  visitor->Trace(watchers_);
  visitor->Trace(one_shots_being_invoked_);
  visitor->Trace(watchers_being_invoked_);
  visitor->Trace(last_position_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

Document* Geolocation::GetDocument() const {
  return To<Document>(GetExecutionContext());
}

LocalFrame* Geolocation::GetFrame() const {
  return GetDocument() ? GetDocument()->GetFrame() : nullptr;
}

void Geolocation::ContextDestroyed(ExecutionContext*) {
  StopTimers();
  one_shots_.clear();
  watchers_->Clear();

  StopUpdating();

  last_position_ = nullptr;
  geolocation_.reset();
  geolocation_service_.reset();
}

void Geolocation::RecordOriginTypeAccess() const {
  DCHECK(GetFrame());

  Document* document = this->GetDocument();
  DCHECK(document);

  // It is required by isSecureContext() but isn't actually used. This could be
  // used later if a warning is shown in the developer console.
  String insecure_origin_msg;
  if (document->IsSecureContext(insecure_origin_msg)) {
    UseCounter::Count(document, WebFeature::kGeolocationSecureOrigin);
    document->CountUseOnlyInCrossOriginIframe(
        WebFeature::kGeolocationSecureOriginIframe);
  } else if (GetFrame()
                 ->GetSettings()
                 ->GetAllowGeolocationOnInsecureOrigins()) {
    // TODO(jww): This should be removed after WebView is fixed so that it
    // disallows geolocation in insecure contexts.
    //
    // See https://crbug.com/603574.
    Deprecation::CountDeprecation(
        document, WebFeature::kGeolocationInsecureOriginDeprecatedNotRemoved);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document,
        WebFeature::kGeolocationInsecureOriginIframeDeprecatedNotRemoved);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kGeolocationInsecureHost);
  } else {
    Deprecation::CountDeprecation(document,
                                  WebFeature::kGeolocationInsecureOrigin);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document, WebFeature::kGeolocationInsecureOriginIframe);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kGeolocationInsecureHost);
  }
}

void Geolocation::getCurrentPosition(V8PositionCallback* success_callback,
                                     V8PositionErrorCallback* error_callback,
                                     const PositionOptions* options) {
  if (!GetFrame())
    return;

  probe::BreakableLocation(GetDocument(), "Geolocation.getCurrentPosition");

  auto* notifier = MakeGarbageCollected<GeoNotifier>(this, success_callback,
                                                     error_callback, options);

  one_shots_.insert(notifier);

  StartRequest(notifier);
}

int Geolocation::watchPosition(V8PositionCallback* success_callback,
                               V8PositionErrorCallback* error_callback,
                               const PositionOptions* options) {
  if (!GetFrame())
    return 0;

  probe::BreakableLocation(GetDocument(), "Geolocation.watchPosition");

  auto* notifier = MakeGarbageCollected<GeoNotifier>(this, success_callback,
                                                     error_callback, options);

  int watch_id;
  // Keep asking for the next id until we're given one that we don't already
  // have.
  do {
    watch_id = GetExecutionContext()->CircularSequentialID();
  } while (!watchers_->Add(watch_id, notifier));

  StartRequest(notifier);

  return watch_id;
}

void Geolocation::StartRequest(GeoNotifier* notifier) {
  RecordOriginTypeAccess();
  String error_message;
  if (!GetFrame()->GetSettings()->GetAllowGeolocationOnInsecureOrigins() &&
      !GetExecutionContext()->IsSecureContext(error_message)) {
    notifier->SetFatalError(MakeGarbageCollected<GeolocationPositionError>(
        GeolocationPositionError::kPermissionDenied, error_message));
    return;
  }

  if (!GetDocument()->IsFeatureEnabled(
          mojom::FeaturePolicyFeature::kGeolocation,
          ReportOptions::kReportOnFailure, kFeaturePolicyConsoleWarning)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kGeolocationDisabledByFeaturePolicy);
    notifier->SetFatalError(MakeGarbageCollected<GeolocationPositionError>(
        GeolocationPositionError::kPermissionDenied,
        kFeaturePolicyErrorMessage));
    return;
  }

  ReportGeolocationViolation(GetDocument());

  if (HaveSuitableCachedPosition(notifier->Options())) {
    notifier->SetUseCachedPosition();
  } else {
    StartUpdating(notifier);
  }
}

void Geolocation::FatalErrorOccurred(GeoNotifier* notifier) {
  DCHECK(!notifier->IsTimerActive());

  // This request has failed fatally. Remove it from our lists.
  one_shots_.erase(notifier);
  watchers_->Remove(notifier);

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::RequestUsesCachedPosition(GeoNotifier* notifier) {
  DCHECK(!notifier->IsTimerActive());

  notifier->RunSuccessCallback(last_position_);

  // If this is a one-shot request, stop it. Otherwise, if the watch still
  // exists, start the service to get updates.
  if (one_shots_.Contains(notifier)) {
    one_shots_.erase(notifier);
  } else if (watchers_->Contains(notifier)) {
    StartUpdating(notifier);
  }

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::RequestTimedOut(GeoNotifier* notifier) {
  DCHECK(!notifier->IsTimerActive());

  // If this is a one-shot request, stop it.
  one_shots_.erase(notifier);

  if (!HasListeners())
    StopUpdating();
}

bool Geolocation::DoesOwnNotifier(GeoNotifier* notifier) const {
  return one_shots_.Contains(notifier) ||
         one_shots_being_invoked_.Contains(notifier) ||
         watchers_->Contains(notifier) ||
         watchers_being_invoked_.Contains(notifier);
}

bool Geolocation::HaveSuitableCachedPosition(const PositionOptions* options) {
  if (!last_position_)
    return false;
  if (!options->maximumAge())
    return false;
  DOMTimeStamp current_time_millis =
      ConvertSecondsToDOMTimeStamp(base::Time::Now().ToDoubleT());
  return last_position_->timestamp() >
         current_time_millis - options->maximumAge();
}

void Geolocation::clearWatch(int watch_id) {
  if (watch_id <= 0)
    return;

  GeoNotifier* notifier = watchers_->Find(watch_id);
  if (!notifier)
    return;

  notifier->StopTimer();
  watchers_->Remove(watch_id);

  if (!HasListeners())
    StopUpdating();
}

void Geolocation::StopTimers() {
  for (const auto& notifier : one_shots_) {
    notifier->StopTimer();
  }

  for (const auto& notifier : watchers_->Notifiers()) {
    notifier->StopTimer();
  }
}

void Geolocation::HandleError(GeolocationPositionError* error) {
  DCHECK(error);

  DCHECK(one_shots_being_invoked_.IsEmpty());
  DCHECK(watchers_being_invoked_.IsEmpty());

  if (error->IsFatal()) {
    // Stop the timers of |one_shots_| and |watchers_| before swapping/copying
    // them.
    StopTimers();
  }

  // Set |one_shots_being_invoked_| and |watchers_being_invoked_| to the
  // callbacks to be invoked, which must not change during invocation of
  // the callbacks. Note that |one_shots_| and |watchers_| might be changed
  // by a callback through getCurrentPosition, watchPosition, and/or
  // clearWatch.
  swap(one_shots_, one_shots_being_invoked_);
  watchers_->CopyNotifiersToVector(watchers_being_invoked_);

  if (error->IsFatal()) {
    // Clear the watchers before invoking the callbacks.
    watchers_->Clear();
  }

  // Invoke the callbacks. Do not send a non-fatal error to the notifiers
  // that only need a cached position. Let them receive a cached position
  // later.
  //
  // A notifier may call |clearWatch|, and in that case, that watcher notifier
  // already scheduled must be immediately cancelled according to the spec. But
  // the current implementation doesn't support such case.
  // TODO(mattreynolds): Support watcher cancellation inside notifier callbacks.
  for (auto& notifier : one_shots_being_invoked_) {
    if (error->IsFatal() || !notifier->UseCachedPosition())
      notifier->RunErrorCallback(error);
  }
  for (auto& notifier : watchers_being_invoked_) {
    if (error->IsFatal() || !notifier->UseCachedPosition())
      notifier->RunErrorCallback(error);
  }

  // |HasListeners| doesn't distinguish those notifiers which require a fresh
  // position from those which are okay with a cached position. Perform the
  // check before adding the latter back to |one_shots_|.
  if (!HasListeners())
    StopUpdating();

  if (!error->IsFatal()) {
    // Keep the notifiers that are okay with a cached position in |one_shots_|.
    for (const auto& notifier : one_shots_being_invoked_) {
      if (notifier->UseCachedPosition())
        one_shots_.InsertWithoutTimerCheck(notifier.Get());
      else
        notifier->StopTimer();
    }
    one_shots_being_invoked_.ClearWithoutTimerCheck();
  }

  one_shots_being_invoked_.clear();
  watchers_being_invoked_.clear();
}

void Geolocation::MakeSuccessCallbacks() {
  DCHECK(last_position_);

  DCHECK(one_shots_being_invoked_.IsEmpty());
  DCHECK(watchers_being_invoked_.IsEmpty());

  // Set |one_shots_being_invoked_| and |watchers_being_invoked_| to the
  // callbacks to be invoked, which must not change during invocation of
  // the callbacks. Note that |one_shots_| and |watchers_| might be changed
  // by a callback through getCurrentPosition, watchPosition, and/or
  // clearWatch.
  swap(one_shots_, one_shots_being_invoked_);
  watchers_->CopyNotifiersToVector(watchers_being_invoked_);

  // Invoke the callbacks.
  //
  // A notifier may call |clearWatch|, and in that case, that watcher notifier
  // already scheduled must be immediately cancelled according to the spec. But
  // the current implementation doesn't support such case.
  // TODO(mattreynolds): Support watcher cancellation inside notifier callbacks.
  for (auto& notifier : one_shots_being_invoked_)
    notifier->RunSuccessCallback(last_position_);
  for (auto& notifier : watchers_being_invoked_)
    notifier->RunSuccessCallback(last_position_);

  if (!HasListeners())
    StopUpdating();

  one_shots_being_invoked_.clear();
  watchers_being_invoked_.clear();
}

void Geolocation::PositionChanged() {
  // Stop all currently running timers.
  StopTimers();

  MakeSuccessCallbacks();
}

void Geolocation::StartUpdating(GeoNotifier* notifier) {
  updating_ = true;
  if (notifier->Options()->enableHighAccuracy() && !enable_high_accuracy_) {
    enable_high_accuracy_ = true;
    if (geolocation_)
      geolocation_->SetHighAccuracy(true);
  }
  UpdateGeolocationConnection(notifier);
}

void Geolocation::StopUpdating() {
  updating_ = false;
  UpdateGeolocationConnection(nullptr);
  enable_high_accuracy_ = false;
}

void Geolocation::UpdateGeolocationConnection(GeoNotifier* notifier) {
  if (!GetExecutionContext() || !GetPage() || !GetPage()->IsPageVisible() ||
      !updating_) {
    geolocation_.reset();
    geolocation_service_.reset();
    disconnected_geolocation_ = true;
    return;
  }
  if (geolocation_) {
    if (notifier)
      notifier->StartTimer();
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetFrame()->GetInterfaceProvider().GetInterface(
      geolocation_service_.BindNewPipeAndPassReceiver(task_runner));
  geolocation_service_->CreateGeolocation(
      geolocation_.BindNewPipeAndPassReceiver(std::move(task_runner)),
      LocalFrame::HasTransientUserActivation(GetFrame()),
      WTF::Bind(&Geolocation::OnGeolocationPermissionStatusUpdated,
                WrapWeakPersistent(this), WrapWeakPersistent(notifier)));

  geolocation_.set_disconnect_handler(WTF::Bind(
      &Geolocation::OnGeolocationConnectionError, WrapWeakPersistent(this)));
  if (enable_high_accuracy_)
    geolocation_->SetHighAccuracy(true);
  QueryNextPosition();
}

void Geolocation::QueryNextPosition() {
  geolocation_->QueryNextPosition(
      WTF::Bind(&Geolocation::OnPositionUpdated, WrapPersistent(this)));
}

void Geolocation::OnPositionUpdated(
    device::mojom::blink::GeopositionPtr position) {
  disconnected_geolocation_ = false;
  if (position->valid) {
    last_position_ = CreateGeoposition(*position);
    PositionChanged();
  } else {
    HandleError(
        CreatePositionError(position->error_code, position->error_message));
  }
  if (!disconnected_geolocation_)
    QueryNextPosition();
}

void Geolocation::PageVisibilityChanged() {
  UpdateGeolocationConnection(nullptr);
}

bool Geolocation::HasPendingActivity() const {
  return !one_shots_.IsEmpty() || !one_shots_being_invoked_.IsEmpty() ||
         !watchers_->IsEmpty() || !watchers_being_invoked_.IsEmpty();
}

void Geolocation::OnGeolocationConnectionError() {
  StopUpdating();
  // The only reason that we would fail to get a ConnectionError is if we lack
  // sufficient permission.
  auto* error = MakeGarbageCollected<GeolocationPositionError>(
      GeolocationPositionError::kPermissionDenied,
      kPermissionDeniedErrorMessage);
  error->SetIsFatal(true);
  HandleError(error);
}

void Geolocation::OnGeolocationPermissionStatusUpdated(
    GeoNotifier* notifier,
    mojom::PermissionStatus status) {
  if (notifier && status == mojom::PermissionStatus::GRANTED) {
    // Avoid starting the notifier timer if the notifier has already been
    // removed.
    if (DoesOwnNotifier(notifier))
      notifier->StartTimer();
  }
}

}  // namespace blink
