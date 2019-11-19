// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_DOCUMENT_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_DOCUMENT_RESOURCE_COORDINATOR_H_

#include <memory>

#include "base/macros.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BrowserInterfaceBrokerProxy;

class PLATFORM_EXPORT DocumentResourceCoordinator final {
  USING_FAST_MALLOC(DocumentResourceCoordinator);

 public:
  // Returns nullptr if instrumentation is not enabled.
  static std::unique_ptr<DocumentResourceCoordinator> MaybeCreate(
      const BrowserInterfaceBrokerProxy&);
  ~DocumentResourceCoordinator();

  void SetNetworkAlmostIdle();
  void SetLifecycleState(performance_manager::mojom::LifecycleState);
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload);
  void SetOriginTrialFreezePolicy(
      performance_manager::mojom::InterventionPolicy policy);
  // A one way switch that marks a frame as being an adframe.
  void SetIsAdFrame();
  void OnNonPersistentNotificationCreated();

 private:
  explicit DocumentResourceCoordinator(const BrowserInterfaceBrokerProxy&);

  mojo::Remote<performance_manager::mojom::blink::DocumentCoordinationUnit>
      service_;

  DISALLOW_COPY_AND_ASSIGN(DocumentResourceCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_DOCUMENT_RESOURCE_COORDINATOR_H_
