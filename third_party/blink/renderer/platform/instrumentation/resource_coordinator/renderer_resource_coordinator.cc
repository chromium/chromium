// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

RendererResourceCoordinator* g_renderer_resource_coordinator = nullptr;

class DummyRendererResourceCoordinator final
    : public RendererResourceCoordinator {
 public:
  DummyRendererResourceCoordinator() = default;
  ~DummyRendererResourceCoordinator() final = default;

  static DummyRendererResourceCoordinator* Get() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(DummyRendererResourceCoordinator, instance,
                                    ());
    return &instance;
  }

  // RendererResourceCoordinator:
  void SetMainThreadTaskLoadIsLow(bool) final {}
  void OnScriptStateCreated(ScriptState* script_state,
                            ExecutionContext* execution_context) final {}
  void OnScriptStateDetached(ScriptState* script_state) final {}
  void OnScriptStateDestroyed(ScriptState* script_state) final {}
  void OnBeforeContentFrameAttached(const Frame& frame,
                                    const HTMLFrameOwnerElement& owner) final {}
  void OnBeforeContentFrameDetached(const Frame& frame,
                                    const HTMLFrameOwnerElement& owner) final {}
};

}  // namespace

// static
void RendererResourceCoordinator::Set(RendererResourceCoordinator* instance) {
  g_renderer_resource_coordinator = instance;
}

// static
RendererResourceCoordinator* RendererResourceCoordinator::Get() {
  if (g_renderer_resource_coordinator)
    return g_renderer_resource_coordinator;
  return DummyRendererResourceCoordinator::Get();
}

}  // namespace blink
