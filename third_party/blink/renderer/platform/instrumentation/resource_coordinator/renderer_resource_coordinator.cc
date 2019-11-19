// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

RendererResourceCoordinator* g_renderer_resource_coordinator = nullptr;

}  // namespace

// static
void RendererResourceCoordinator::MaybeInitialize() {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled())
    return;

  blink::Platform* platform = Platform::Current();
  DCHECK(IsMainThread());
  DCHECK(platform);

  mojo::PendingRemote<
      performance_manager::mojom::blink::ProcessCoordinationUnit>
      remote;
  platform->GetBrowserInterfaceBroker()->GetInterface(
      remote.InitWithNewPipeAndPassReceiver());
  g_renderer_resource_coordinator =
      new RendererResourceCoordinator(std::move(remote));
}

// static
void RendererResourceCoordinator::
    SetCurrentRendererResourceCoordinatorForTesting(
        RendererResourceCoordinator* renderer_resource_coordinator) {
  g_renderer_resource_coordinator = renderer_resource_coordinator;
}

// static
RendererResourceCoordinator* RendererResourceCoordinator::Get() {
  return g_renderer_resource_coordinator;
}

RendererResourceCoordinator::RendererResourceCoordinator(
    mojo::PendingRemote<
        performance_manager::mojom::blink::ProcessCoordinationUnit> remote) {
  service_.Bind(std::move(remote));
}

RendererResourceCoordinator::RendererResourceCoordinator() = default;

RendererResourceCoordinator::~RendererResourceCoordinator() = default;

void RendererResourceCoordinator::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  if (!service_)
    return;
  service_->SetExpectedTaskQueueingDuration(duration);
}

void RendererResourceCoordinator::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  if (!service_)
    return;
  service_->SetMainThreadTaskLoadIsLow(main_thread_task_load_is_low);
}

}  // namespace blink
