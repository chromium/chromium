// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_

#include "services/resource_coordinator/public/mojom/coordination_unit.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace blink {

class PLATFORM_EXPORT FrameResourceCoordinator final {
  WTF_MAKE_NONCOPYABLE(FrameResourceCoordinator);

 public:
  static std::unique_ptr<FrameResourceCoordinator> Create(
      service_manager::InterfaceProvider*);
  ~FrameResourceCoordinator();

  void SetNetworkAlmostIdle(bool);
  void SetLifecycleState(resource_coordinator::mojom::LifecycleState);
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload);
  void OnNonPersistentNotificationCreated();

 private:
  explicit FrameResourceCoordinator(service_manager::InterfaceProvider*);

  resource_coordinator::mojom::blink::FrameCoordinationUnitPtr service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_FRAME_RESOURCE_COORDINATOR_H_
