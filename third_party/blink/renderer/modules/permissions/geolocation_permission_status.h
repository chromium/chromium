// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_GEOLOCATION_PERMISSION_STATUS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_GEOLOCATION_PERMISSION_STATUS_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/permissions/permission_status.h"
#include "third_party/blink/renderer/modules/permissions/permission_status_listener.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;

// Expose the status of a given permission type for the current
// ExecutionContext.
class GeolocationPermissionStatus final : public PermissionStatus {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GeolocationPermissionStatus(PermissionStatusListener*, ExecutionContext*);
  ~GeolocationPermissionStatus() override;

  std::optional<V8AccuracyMode> accuracyMode() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_GEOLOCATION_PERMISSION_STATUS_H_
