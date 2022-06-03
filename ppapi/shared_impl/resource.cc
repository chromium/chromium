// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource.h"

#include "base/check.h"
#include "base/notreached.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {

Resource::Resource(ResourceObjectType type, PP_Instance instance)
    : host_resource_(HostResource::MakeInstanceOnly(instance)) {
  // The instance should be valid (nonzero).
  DCHECK(instance);

  pp_resource_ = PpapiGlobals::Get()->GetResourceTracker()->AddResource(this);
  if (type == OBJECT_IS_IMPL) {
    // For the in-process case, the host resource and resource are the same.
    //
    // Note that we need to have set the instance above (in the initializer
    // list) since AddResource needs our instance() getter to work, and that
    // goes through the host resource. When we get the "real" resource ID,
    // we re-set the host_resource.
    host_resource_.SetHostResource(instance, pp_resource_);
  }
}

Resource::Resource(ResourceObjectType type, const HostResource& host_resource)
    : host_resource_(host_resource) {
  pp_resource_ = PpapiGlobals::Get()->GetResourceTracker()->AddResource(this);
  if (type == OBJECT_IS_IMPL) {
    // When using this constructor for the implementation, the resource ID
    // should not have been passed in.
    DCHECK(host_resource_.host_resource() == 0);

    // See previous constructor.
    host_resource_.SetHostResource(host_resource.instance(), pp_resource_);
  }
}

Resource::Resource(Untracked) {
  pp_resource_ = PpapiGlobals::Get()->GetResourceTracker()->AddResource(this);
}

Resource::~Resource() { RemoveFromResourceTracker(); }

PP_Resource Resource::GetReference() {
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(pp_resource());
  return pp_resource();
}

void Resource::NotifyLastPluginRefWasDeleted() {
  // Notify subclasses.
  LastPluginRefWasDeleted();
}

void Resource::NotifyInstanceWasDeleted() {
  // Hold a reference, because InstanceWasDeleted() may cause us to be
  // destroyed.
  scoped_refptr<Resource> keep_alive(this);

  // Notify subclasses.
  InstanceWasDeleted();

  host_resource_ = HostResource();
}

void Resource::OnReplyReceived(const proxy::ResourceMessageReplyParams& params,
                               const IPC::Message& msg) {
  NOTREACHED();
}

void Resource::Log(PP_LogLevel level, const std::string& message) {
  PpapiGlobals::Get()->LogWithSource(
      pp_instance(), level, std::string(), message);
}

void Resource::RemoveFromResourceTracker() {
  PpapiGlobals::Get()->GetResourceTracker()->RemoveResource(this);
}

#define DEFINE_TYPE_GETTER(RESOURCE) \
  thunk::RESOURCE* Resource::As##RESOURCE() { return NULL; }
FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

}  // namespace ppapi
