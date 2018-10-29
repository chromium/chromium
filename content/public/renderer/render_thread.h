// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include "base/callback.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/blink/public/platform/web_string.h"

class GURL;

namespace base {
class WaitableEvent;
}

namespace blink {
namespace scheduler {
enum class RendererProcessType;
}
}  // namespace blink

namespace IPC {
class MessageFilter;
class SyncChannel;
class SyncMessageFilter;
}

namespace v8 {
class Extension;
}

namespace content {

class RenderThreadObserver;
class ResourceDispatcherDelegate;

class CONTENT_EXPORT RenderThread : virtual public ChildThread {
 public:
  // Returns the one render thread for this process.  Note that this can only
  // be accessed when running on the render thread itself.
  static RenderThread* Get();

  RenderThread();
  ~RenderThread() override;

  virtual IPC::SyncChannel* GetChannel() = 0;
  virtual std::string GetLocale() = 0;
  virtual IPC::SyncMessageFilter* GetSyncMessageFilter() = 0;

  // Called to add or remove a listener for a particular message routing ID.
  // These methods normally get delegated to a MessageRouter.
  virtual void AddRoute(int32_t routing_id, IPC::Listener* listener) = 0;
  virtual void RemoveRoute(int32_t routing_id) = 0;
  virtual int GenerateRoutingID() = 0;

  // These map to IPC::ChannelProxy methods.
  virtual void AddFilter(IPC::MessageFilter* filter) = 0;
  virtual void RemoveFilter(IPC::MessageFilter* filter) = 0;

  // Add/remove observers for the process.
  virtual void AddObserver(RenderThreadObserver* observer) = 0;
  virtual void RemoveObserver(RenderThreadObserver* observer) = 0;

  // Set the ResourceDispatcher delegate object for this process.
  virtual void SetResourceDispatcherDelegate(
      ResourceDispatcherDelegate* delegate) = 0;

  // Asks the host to create a block of shared memory for the renderer.
  // The shared memory allocated by the host is returned back.
  virtual std::unique_ptr<base::SharedMemory> HostAllocateSharedMemoryBuffer(
      size_t buffer_size) = 0;

  // Registers the given V8 extension with WebKit.
  virtual void RegisterExtension(v8::Extension* extension) = 0;

  // Post task to all worker threads. Returns number of workers.
  virtual int PostTaskToAllWebWorkers(const base::Closure& closure) = 0;

  // Resolve the proxy servers to use for a given url. On success true is
  // returned and |proxy_list| is set to a PAC string containing a list of
  // proxy servers.
  virtual bool ResolveProxy(const GURL& url, std::string* proxy_list) = 0;

  // Gets the shutdown event for the process.
  virtual base::WaitableEvent* GetShutdownEvent() = 0;

  // Retrieve the process ID of the browser process.
  virtual int32_t GetClientId() = 0;

  // Get the online status of the browser - false when there is no network
  // access.
  virtual bool IsOnline() = 0;

  // Set the renderer process type.
  virtual void SetRendererProcessType(
      blink::scheduler::RendererProcessType type) = 0;

  // Returns the user-agent string.
  virtual blink::WebString GetUserAgent() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_THREAD_H_
