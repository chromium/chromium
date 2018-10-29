// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_

#include "services/resource_coordinator/public/mojom/coordination_unit.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace blink {

class PLATFORM_EXPORT RendererResourceCoordinator {
  WTF_MAKE_NONCOPYABLE(RendererResourceCoordinator);

 public:
  static void Initialize();
  static RendererResourceCoordinator& Get();

  // Used to switch the current renderer resource coordinator only for testing.
  static void SetCurrentRendererResourceCoordinatorForTesting(
      RendererResourceCoordinator*);

  ~RendererResourceCoordinator();

  void SetExpectedTaskQueueingDuration(base::TimeDelta);
  void SetMainThreadTaskLoadIsLow(bool);
  void OnRendererIsBloated();

 protected:
  RendererResourceCoordinator();

 private:
  RendererResourceCoordinator(service_manager::Connector*, const std::string&);

  resource_coordinator::mojom::blink::ProcessCoordinationUnitPtr service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
