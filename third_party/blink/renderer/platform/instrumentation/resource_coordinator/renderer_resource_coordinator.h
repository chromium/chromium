// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_

#include "base/macros.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT RendererResourceCoordinator {
  USING_FAST_MALLOC(RendererResourceCoordinator);

 public:
  // Only initializes if the instrumentation runtime feature is enabled.
  static void MaybeInitialize();
  static RendererResourceCoordinator* Get();

  // Used to switch the current renderer resource coordinator only for testing.
  static void SetCurrentRendererResourceCoordinatorForTesting(
      RendererResourceCoordinator*);

  ~RendererResourceCoordinator();

  void SetExpectedTaskQueueingDuration(base::TimeDelta);
  void SetMainThreadTaskLoadIsLow(bool);

 protected:
  RendererResourceCoordinator();

 private:
  explicit RendererResourceCoordinator(
      mojo::PendingRemote<
          performance_manager::mojom::blink::ProcessCoordinationUnit> remote);

  mojo::Remote<performance_manager::mojom::blink::ProcessCoordinationUnit>
      service_;

  DISALLOW_COPY_AND_ASSIGN(RendererResourceCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_RESOURCE_COORDINATOR_RENDERER_RESOURCE_COORDINATOR_H_
