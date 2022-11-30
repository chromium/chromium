// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

#include <map>
#include <unordered_set>
#include <utility>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace ppapi {

namespace proxy {

class PPAPI_PROXY_EXPORT PluginResourceTracker : public ResourceTracker {
 public:
  PluginResourceTracker();

  PluginResourceTracker(const PluginResourceTracker&) = delete;
  PluginResourceTracker& operator=(const PluginResourceTracker&) = delete;

  ~PluginResourceTracker() override;

  // Given a host resource, maps it to an existing plugin resource ID if it
  // exists, or returns 0 on failure.
  PP_Resource PluginResourceForHostResource(
      const HostResource& resource) const;

  // "Abandons" a PP_Resource on the plugin side. This releases a reference to
  // the resource and allows the plugin side of the resource (the proxy
  // resource) to be destroyed without sending a message to the renderer
  // notifing it that the plugin has released the resource. This is useful when
  // the plugin sends a resource to the renderer in reply to a sync IPC. The
  // plugin would want to release its reference to the reply resource straight
  // away but doing so can sometimes cause the resource to be deleted in the
  // renderer before the sync IPC reply has been received giving the renderer a
  // chance to add a ref to it. (see e.g. crbug.com/490611). Instead the
  // renderer assumes responsibility for the ref that the plugin created and
  // this function can be called.
  void AbandonResource(PP_Resource res);

 protected:
  // ResourceTracker overrides.
  PP_Resource AddResource(Resource* object) override;
  void RemoveResource(Resource* object) override;

 private:
  // Map of host instance/resource pairs to a plugin resource ID.
  typedef std::map<HostResource, PP_Resource> HostResourceMap;
  HostResourceMap host_resource_map_;

  std::unordered_set<PP_Resource> abandoned_resources_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
