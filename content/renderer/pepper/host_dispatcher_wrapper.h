// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_HOST_DISPATCHER_WRAPPER_H_
#define CONTENT_RENDERER_PEPPER_HOST_DISPATCHER_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "content/renderer/pepper/pepper_hung_plugin_filter.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace IPC {
struct ChannelHandle;
}

namespace content {
class PepperHungPluginFilter;
class PluginModule;

// This class wraps a dispatcher and has the same lifetime. A dispatcher has
// the same lifetime as a plugin module, which is longer than any particular
// `blink::WebView` or plugin instance.
class HostDispatcherWrapper {
 public:
  HostDispatcherWrapper(PluginModule* module,
                        base::ProcessId peer_pid,
                        int plugin_child_id,
                        const ppapi::PpapiPermissions& perms,
                        bool is_external);
  virtual ~HostDispatcherWrapper();

  bool Init(const IPC::ChannelHandle& channel_handle,
            PP_GetInterface_Func local_get_interface,
            const ppapi::Preferences& preferences,
            scoped_refptr<PepperHungPluginFilter> filter,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Implements GetInterface for the proxied plugin.
  const void* GetProxiedInterface(const char* name);

  // Notification to the out-of-process layer that the given plugin instance
  // has been created. This will happen before the normal PPB_Instance method
  // calls so the out-of-process code can set up the tracking information for
  // the new instance.
  void AddInstance(PP_Instance instance);

  // Like AddInstance but removes the given instance. This is called after
  // regular instance shutdown so the out-of-process code can clean up its
  // tracking information.
  void RemoveInstance(PP_Instance instance);

  base::ProcessId peer_pid() { return peer_pid_; }
  int plugin_child_id() { return plugin_child_id_; }
  ppapi::proxy::HostDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  raw_ptr<PluginModule> module_;

  base::ProcessId peer_pid_;

  // ID that the browser process uses to idetify the child process for the
  // plugin. This isn't directly useful from our process (the renderer) except
  // in messages to the browser to disambiguate plugins.
  int plugin_child_id_;

  ppapi::PpapiPermissions permissions_;
  bool is_external_;

  std::unique_ptr<ppapi::proxy::HostDispatcher> dispatcher_;
  std::unique_ptr<ppapi::proxy::ProxyChannel::Delegate> dispatcher_delegate_;
  // We hold the hung_plugin_filter_ to guarantee it outlives |dispatcher_|,
  // since it is an observer of |dispatcher_| for sync calls.
  scoped_refptr<PepperHungPluginFilter> hung_plugin_filter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_HOST_DISPATCHER_WRAPPER_H_
