// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_tracker.h"

#include "base/check.h"
#include "base/memory/singleton.h"
#include "base/not_fatal_until.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

PluginResourceTracker::PluginResourceTracker() : ResourceTracker(THREAD_SAFE) {
  UseOddResourceValueInDebugMode();
}

PluginResourceTracker::~PluginResourceTracker() {
}

PP_Resource PluginResourceTracker::PluginResourceForHostResource(
    const HostResource& resource) const {
  HostResourceMap::const_iterator found = host_resource_map_.find(resource);
  if (found == host_resource_map_.end())
    return 0;
  return found->second;
}

void PluginResourceTracker::AbandonResource(PP_Resource res) {
  DCHECK(GetResource(res));
  bool inserted = abandoned_resources_.insert(res).second;
  DCHECK(inserted);

  ReleaseResource(res);
}

PP_Resource PluginResourceTracker::AddResource(Resource* object) {
  // If there's a HostResource, it must not be added twice.
  DCHECK(!object->host_resource().host_resource() ||
         (host_resource_map_.find(object->host_resource()) ==
          host_resource_map_.end()));

  PP_Resource ret = ResourceTracker::AddResource(object);

  // Some resources are plugin-only, so they don't have a host resource.
  if (object->host_resource().host_resource())
    host_resource_map_.insert(std::make_pair(object->host_resource(), ret));
  return ret;
}

void PluginResourceTracker::RemoveResource(Resource* object) {
  ResourceTracker::RemoveResource(object);

  if (!object->host_resource().is_null()) {
    // The host_resource will be NULL for proxy-only resources, which we
    // obviously don't need to tell the host about.
    CHECK(host_resource_map_.find(object->host_resource()) !=
              host_resource_map_.end(),
          base::NotFatalUntil::M130);
    host_resource_map_.erase(object->host_resource());

    bool abandoned = false;
    auto it = abandoned_resources_.find(object->pp_resource());
    if (it != abandoned_resources_.end()) {
      abandoned = true;
      abandoned_resources_.erase(it);
    }

    PluginDispatcher* dispatcher =
        PluginDispatcher::GetForInstance(object->pp_instance());
    if (dispatcher && !abandoned) {
      // The dispatcher can be NULL if the plugin held on to a resource after
      // the instance was destroyed. In that case the browser-side resource has
      // already been freed correctly on the browser side.
      dispatcher->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
          API_ID_PPB_CORE, object->host_resource()));
    }
  }
}

}  // namespace proxy
}  // namespace ppapi
