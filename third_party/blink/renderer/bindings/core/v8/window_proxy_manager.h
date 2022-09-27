// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WINDOW_PROXY_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WINDOW_PROXY_MANAGER_H_

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/remote_window_proxy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class LocalFrame;
class SecurityOrigin;

class CORE_EXPORT WindowProxyManager
    : public GarbageCollected<WindowProxyManager> {
 public:
  void Trace(Visitor*) const;

  v8::Isolate* GetIsolate() const { return isolate_; }

  void ClearForClose();
  void ClearForNavigation();
  void ClearForSwap();
  void ClearForV8MemoryPurge();

  // Helpers used to transfer global proxies from the previous frame to the new
  // frame when swapping frames. Global proxies are passed in a vector to ensure
  // the main world is always processed first. This is needed to prevent bugs
  // like https://crbug.com/700077.
  using GlobalProxyVector =
      Vector<std::pair<DOMWrapperWorld*, v8::Local<v8::Object>>>;
  void ReleaseGlobalProxies(GlobalProxyVector&);
  void SetGlobalProxies(const GlobalProxyVector&);

  WindowProxy* GetWindowProxy(DOMWrapperWorld& world) {
    WindowProxy* window_proxy = WindowProxyMaybeUninitialized(world);
    window_proxy->InitializeIfNeeded();
    return window_proxy;
  }

  WindowProxy* GetWindowProxyMaybeUninitialized(DOMWrapperWorld& world) {
    WindowProxy* window_proxy = WindowProxyMaybeUninitialized(world);
    return window_proxy;
  }

  void ResetIsolatedWorldsForTesting();

 protected:
  using IsolatedWorldMap = HeapHashMap<int, Member<WindowProxy>>;
  enum class FrameType { kLocal, kRemote };

  WindowProxyManager(Frame&, FrameType);

 private:
  // Creates an uninitialized WindowProxy.
  WindowProxy* CreateWindowProxy(DOMWrapperWorld&);
  WindowProxy* WindowProxyMaybeUninitialized(DOMWrapperWorld&);

  v8::Isolate* const isolate_;
  const Member<Frame> frame_;
  const FrameType frame_type_;

 protected:
  const Member<WindowProxy> window_proxy_;
  IsolatedWorldMap isolated_worlds_;
};

template <typename FrameType, typename ProxyType>
class WindowProxyManagerImplHelper : public WindowProxyManager {
 private:
  using Base = WindowProxyManager;

 public:
  ProxyType* WindowProxy(DOMWrapperWorld& world) {
    return static_cast<ProxyType*>(Base::GetWindowProxy(world));
  }

 protected:
  WindowProxyManagerImplHelper(Frame& frame, FrameType frame_type)
      : WindowProxyManager(frame, frame_type) {}
};

class LocalWindowProxyManager
    : public WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy> {
 public:
  explicit LocalWindowProxyManager(LocalFrame& frame)
      : WindowProxyManagerImplHelper<LocalFrame, LocalWindowProxy>(
            frame,
            FrameType::kLocal) {}

  // TODO(yukishiino): Remove this method.
  LocalWindowProxy* MainWorldProxyMaybeUninitialized() {
    return static_cast<LocalWindowProxy*>(window_proxy_.Get());
  }

  void UpdateDocument();

  // Sets the given security origin to the main world's context.  Also updates
  // the security origin of the context for each isolated world.
  void UpdateSecurityOrigin(const SecurityOrigin*);

  void SetAbortScriptExecution(
      v8::Context::AbortScriptExecutionCallback callback);
};

class RemoteWindowProxyManager
    : public WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy> {
 public:
  explicit RemoteWindowProxyManager(RemoteFrame& frame)
      : WindowProxyManagerImplHelper<RemoteFrame, RemoteWindowProxy>(
            frame,
            FrameType::kRemote) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_WINDOW_PROXY_MANAGER_H_
