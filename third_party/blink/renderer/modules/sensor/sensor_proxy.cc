// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

const char SensorProxy::kDefaultErrorDescription[] =
    "Could not connect to a sensor";

SensorProxy::SensorProxy(device::mojom::blink::SensorType sensor_type,
                         SensorProviderProxy* provider,
                         Page* page)
    : PageVisibilityObserver(page),
      FocusChangedObserver(page),
      type_(sensor_type),
      state_(SensorProxy::kUninitialized),
      provider_(provider) {}

SensorProxy::~SensorProxy() {}

void SensorProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
  visitor->Trace(provider_);
  PageVisibilityObserver::Trace(visitor);
  FocusChangedObserver::Trace(visitor);
}

void SensorProxy::AddObserver(Observer* observer) {
  if (!observers_.Contains(observer))
    observers_.insert(observer);
}

void SensorProxy::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}

void SensorProxy::Detach() {
  if (!detached_) {
    provider_->RemoveSensorProxy(this);
    detached_ = true;
  }
}

void SensorProxy::ReportError(DOMExceptionCode code, const String& message) {
  auto copy = observers_;
  for (Observer* observer : copy) {
    observer->OnSensorError(code, message, String());
  }
}

namespace {

uint16_t GetScreenOrientationAngle(LocalFrame& frame) {
  if (WebTestSupport::IsRunningWebTest()) {
    // Simulate that the device is turned 90 degrees on the right.
    // 'orientation_angle' must be 270 as per
    // https://w3c.github.io/screen-orientation/#dfn-update-the-orientation-information.
    return 270;
  }
  return frame.GetChromeClient().GetScreenInfo(frame).orientation_angle;
}

}  // namespace

const device::SensorReading& SensorProxy::GetReading(bool remapped) const {
  DCHECK(IsInitialized());
  if (remapped) {
    if (remapped_reading_.timestamp() != reading_.timestamp()) {
      remapped_reading_ = reading_;
      SensorReadingRemapper::RemapToScreenCoords(
          type_,
          GetScreenOrientationAngle(
              *provider_->GetSupplementable()->GetFrame()),
          &remapped_reading_);
    }
    return remapped_reading_;
  }
  return reading_;
}

void SensorProxy::PageVisibilityChanged() {
  UpdateSuspendedStatus();
}

void SensorProxy::FocusedFrameChanged() {
  UpdateSuspendedStatus();
}

void SensorProxy::UpdateSuspendedStatus() {
  if (!IsInitialized())
    return;

  if (ShouldSuspendUpdates())
    Suspend();
  else
    Resume();
}

bool SensorProxy::ShouldSuspendUpdates() const {
  if (!GetPage()->IsPageVisible())
    return true;

  LocalFrame* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  LocalFrame* this_frame = provider_->GetSupplementable()->GetFrame();

  if (!focused_frame || !this_frame)
    return true;

  if (focused_frame == this_frame)
    return false;

  const SecurityOrigin* focused_frame_origin =
      focused_frame->GetSecurityContext()->GetSecurityOrigin();
  const SecurityOrigin* this_origin =
      this_frame->GetSecurityContext()->GetSecurityOrigin();

  return !focused_frame_origin->CanAccess(this_origin);
}

device::mojom::blink::SensorProvider* SensorProxy::sensor_provider() const {
  return provider_->sensor_provider();
}

}  // namespace blink
