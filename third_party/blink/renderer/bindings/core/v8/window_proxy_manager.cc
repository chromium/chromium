// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"

#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/remote_window_proxy.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

void WindowProxyManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(window_proxy_);
  visitor->Trace(isolated_worlds_);
}

void WindowProxyManager::ClearForClose() {
  window_proxy_->ClearForClose();
  for (auto& entry : isolated_worlds_)
    entry.value->ClearForClose();
}

void WindowProxyManager::ClearForNavigation() {
  window_proxy_->ClearForNavigation();
  for (auto& entry : isolated_worlds_)
    entry.value->ClearForNavigation();
}

void WindowProxyManager::ClearForSwap() {
  window_proxy_->ClearForSwap();
  for (auto& entry : isolated_worlds_)
    entry.value->ClearForSwap();
}

void WindowProxyManager::ClearForV8MemoryPurge() {
  window_proxy_->ClearForV8MemoryPurge();
  for (auto& entry : isolated_worlds_)
    entry.value->ClearForV8MemoryPurge();
}

void WindowProxyManager::ReleaseGlobalProxies(
    GlobalProxyVector& global_proxies) {
  DCHECK(global_proxies.empty());
  global_proxies.ReserveInitialCapacity(1 + isolated_worlds_.size());
  global_proxies.emplace_back(&window_proxy_->World(),
                              window_proxy_->ReleaseGlobalProxy());
  for (auto& entry : isolated_worlds_) {
    global_proxies.emplace_back(
        &entry.value->World(),
        WindowProxyMaybeUninitialized(entry.value->World())
            ->ReleaseGlobalProxy());
  }
}

void WindowProxyManager::SetGlobalProxies(
    const GlobalProxyVector& global_proxies) {
  for (const auto& entry : global_proxies)
    WindowProxyMaybeUninitialized(*entry.first)->SetGlobalProxy(entry.second);

  // Any transferred global proxies must now be reinitialized to ensure any
  // preexisting JS references to global proxies don't break.

  // For local frames, the global proxies cannot be reinitialized yet. Blink is
  // in the midst of committing a navigation and swapping in the new frame.
  // Instead, the global proxies will be reinitialized after this via a call to
  // `UpdateDocument()` when the new `Document` is installed: this will happen
  // before committing the navigation completes and yields back to the event
  // loop.
  if (frame_type_ == FrameType::kLocal)
    return;

  for (const auto& entry : global_proxies) {
    WindowProxyMaybeUninitialized(*entry.first)->InitializeIfNeeded();
  }
}

void WindowProxyManager::ResetIsolatedWorldsForTesting() {
  for (auto& world_info : isolated_worlds_) {
    world_info.value->ClearForClose();
  }
  isolated_worlds_.clear();
}

WindowProxyManager::WindowProxyManager(Frame& frame, FrameType frame_type)
    : isolate_(V8PerIsolateData::MainThreadIsolate()),
      frame_(&frame),
      frame_type_(frame_type),
      window_proxy_(CreateWindowProxy(DOMWrapperWorld::MainWorld())) {
  // All WindowProxyManagers must be created in the main thread.
  // Note that |isolate_| is initialized with
  // V8PerIsolateData::MainThreadIsolate().
  CHECK(IsMainThread());
}

WindowProxy* WindowProxyManager::CreateWindowProxy(DOMWrapperWorld& world) {
  switch (frame_type_) {
    case FrameType::kLocal:
      // Directly use static_cast instead of toLocalFrame because
      // WindowProxyManager gets instantiated during a construction of
      // LocalFrame and at that time virtual member functions are not yet
      // available (we cannot use LocalFrame::isLocalFrame).  Ditto for
      // RemoteFrame.
      return MakeGarbageCollected<LocalWindowProxy>(
          isolate_, *static_cast<LocalFrame*>(frame_.Get()), &world);
    case FrameType::kRemote:
      return MakeGarbageCollected<RemoteWindowProxy>(
          isolate_, *static_cast<RemoteFrame*>(frame_.Get()), &world);
  }
  NOTREACHED();
  return nullptr;
}

WindowProxy* WindowProxyManager::WindowProxyMaybeUninitialized(
    DOMWrapperWorld& world) {
  WindowProxy* window_proxy = nullptr;
  if (world.IsMainWorld()) {
    window_proxy = window_proxy_.Get();
  } else {
    IsolatedWorldMap::iterator iter = isolated_worlds_.find(world.GetWorldId());
    if (iter != isolated_worlds_.end()) {
      window_proxy = iter->value.Get();
    } else {
      window_proxy = CreateWindowProxy(world);
      isolated_worlds_.Set(world.GetWorldId(), window_proxy);
    }
  }
  return window_proxy;
}

void LocalWindowProxyManager::UpdateDocument() {
  MainWorldProxyMaybeUninitialized()->UpdateDocument();

  for (auto& entry : isolated_worlds_) {
    To<LocalWindowProxy>(entry.value.Get())->UpdateDocument();
  }
}

void LocalWindowProxyManager::UpdateSecurityOrigin(
    const SecurityOrigin* security_origin) {
  To<LocalWindowProxy>(window_proxy_.Get())
      ->UpdateSecurityOrigin(security_origin);

  for (auto& entry : isolated_worlds_) {
    auto* isolated_window_proxy = To<LocalWindowProxy>(entry.value.Get());
    scoped_refptr<SecurityOrigin> isolated_security_origin =
        isolated_window_proxy->World().IsolatedWorldSecurityOrigin(
            security_origin->AgentClusterId());
    isolated_window_proxy->UpdateSecurityOrigin(isolated_security_origin.get());
  }
}

void LocalWindowProxyManager::SetAbortScriptExecution(
    v8::Context::AbortScriptExecutionCallback callback) {
  v8::HandleScope handle_scope(GetIsolate());

  static_cast<LocalWindowProxy*>(window_proxy_.Get())
      ->SetAbortScriptExecution(callback);

  for (auto& entry : isolated_worlds_) {
    To<LocalWindowProxy>(entry.value.Get())->SetAbortScriptExecution(callback);
  }
}

}  // namespace blink
