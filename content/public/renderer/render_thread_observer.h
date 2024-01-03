// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_

#include "content/common/buildflags.h"
#include "content/common/content_export.h"

namespace blink {
class AssociatedInterfaceRegistry;
}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
namespace IPC {
class Message;
}
#endif

namespace content {

// Base class for objects that want to filter control IPC messages and get
// notified of events.
class CONTENT_EXPORT RenderThreadObserver {
 public:
  RenderThreadObserver() {}

  RenderThreadObserver(const RenderThreadObserver&) = delete;
  RenderThreadObserver& operator=(const RenderThreadObserver&) = delete;

  virtual ~RenderThreadObserver() {}

  // Allows handling incoming Mojo requests.
  virtual void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) {}
  virtual void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) {}

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // Allows filtering of control messages.
  virtual bool OnControlMessageReceived(const IPC::Message& message);
#endif

  // Called when the renderer cache of the plugin list has changed.
  virtual void PluginListChanged() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_THREAD_OBSERVER_H_
