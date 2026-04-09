// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/geolocation_permission_status.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_permission_state.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/permissions/permission_status_listener.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"

namespace blink {

GeolocationPermissionStatus::GeolocationPermissionStatus(
    PermissionStatusListener* listener,
    ExecutionContext* execution_context)
    : PermissionStatus(listener, execution_context) {}

GeolocationPermissionStatus::~GeolocationPermissionStatus() = default;

std::optional<V8AccuracyMode> GeolocationPermissionStatus::accuracyMode()
    const {
  if (!listener_ || !listener_->details()) {
    return std::nullopt;
  }
  switch (listener_->details()->which()) {
    case mojom::blink::PermissionDetails::Tag::kGeolocationAccuracy:
      return ToV8AccuracyMode(listener_->details()->get_geolocation_accuracy());
  }
}

}  // namespace blink
