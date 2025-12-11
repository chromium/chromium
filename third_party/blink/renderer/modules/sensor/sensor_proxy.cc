// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "ui/display/screen_info.h"

namespace blink {

const char SensorProxy::kDefaultErrorDescription[] =
    "Could not connect to a sensor";

SensorProxy::SensorProxy(device::mojom::blink::SensorType sensor_type,
                         SensorProviderProxy* provider,
                         Page* page)
    : PageVisibilityObserver(page),
      FocusChangedObserver(page),
      type_(sensor_type),
      provider_(provider) {}

SensorProxy::~SensorProxy() = default;

void SensorProxy::Trace(Visitor* visitor) const {
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

const device::SensorReading& SensorProxy::GetReading(bool remapped) const {
  DCHECK(IsInitialized());
  if (remapped) {
    if (remapped_reading_.timestamp() != reading_.timestamp()) {
      remapped_reading_ = reading_;
      LocalFrame& frame = *provider_->GetSupplementable()->GetFrame();
      SensorReadingRemapper::RemapToScreenCoords(
          type_, frame.GetChromeClient().GetScreenInfo(frame).orientation_angle,
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

  const FocusController& focus_controller = GetPage()->GetFocusController();
  if (!focus_controller.IsFocused()) {
    return true;
  }

  LocalFrame* focused_frame = focus_controller.FocusedFrame();
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

}  // namespace blink
