// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_tracker.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

ResourceTracker::ResourceTracker(ThreadMode thread_mode)
    : last_resource_value_(0) {
  if (thread_mode == SINGLE_THREADED)
    thread_checker_.reset(new base::ThreadChecker);
}

ResourceTracker::~ResourceTracker() {}

void ResourceTracker::CheckThreadingPreconditions() const {
  DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
#ifndef NDEBUG
  ProxyLock::AssertAcquired();
#endif
}

Resource* ResourceTracker::GetResource(PP_Resource res) const {
  CheckThreadingPreconditions();
  ResourceMap::const_iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return NULL;
  return i->second.first;
}

void ResourceTracker::AddRefResource(PP_Resource res) {
  CheckThreadingPreconditions();
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";

  DCHECK(CanOperateOnResource(res));

  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return;

  // Prevent overflow of refcount.
  if (i->second.second ==
      std::numeric_limits<ResourceAndRefCount::second_type>::max())
    return;

  // When we go from 0 to 1 plugin ref count, keep an additional "real" ref
  // on its behalf.
  if (i->second.second == 0)
    i->second.first->AddRef();

  i->second.second++;
  return;
}

void ResourceTracker::ReleaseResource(PP_Resource res) {
  CheckThreadingPreconditions();
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";

  DCHECK(CanOperateOnResource(res));

  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return;

  // Prevent underflow of refcount.
  if (i->second.second == 0)
    return;

  i->second.second--;
  if (i->second.second == 0) {
    LastPluginRefWasDeleted(i->second.first);

    // When we go from 1 to 0 plugin ref count, free the additional "real" ref
    // on its behalf. THIS WILL MOST LIKELY RELEASE THE OBJECT AND REMOVE IT
    // FROM OUR LIST.
    i->second.first->Release();
  }
}

void ResourceTracker::DidCreateInstance(PP_Instance instance) {
  CheckThreadingPreconditions();
  // Due to the infrastructure of some tests, the instance is registered
  // twice in a few cases. It would be nice not to do that and assert here
  // instead.
  if (instance_map_.find(instance) != instance_map_.end())
    return;
  instance_map_[instance] = base::WrapUnique(new InstanceData);
}

void ResourceTracker::DidDeleteInstance(PP_Instance instance) {
  CheckThreadingPreconditions();
  InstanceMap::iterator found_instance = instance_map_.find(instance);

  // Due to the infrastructure of some tests, the instance is unregistered
  // twice in a few cases. It would be nice not to do that and assert here
  // instead.
  if (found_instance == instance_map_.end())
    return;

  InstanceData& data = *found_instance->second;

  // Force release all plugin references to resources associated with the
  // deleted instance. Make a copy since as we iterate through them, each one
  // will remove itself from the tracking info individually.
  ResourceSet to_delete = data.resources;
  ResourceSet::iterator cur = to_delete.begin();
  while (cur != to_delete.end()) {
    // Note that it's remotely possible for the object to already be deleted
    // from the live resources. One case is if a resource object is holding
    // the last ref to another. When we release the first one, it will release
    // the second one. So the second one will be gone when we eventually get
    // to it.
    ResourceMap::iterator found_resource = live_resources_.find(*cur);
    if (found_resource != live_resources_.end()) {
      Resource* resource = found_resource->second.first;
      if (found_resource->second.second > 0) {
        LastPluginRefWasDeleted(resource);
        found_resource->second.second = 0;

        // This will most likely delete the resource object and remove it
        // from the live_resources_ list.
        resource->Release();
      }
    }

    cur++;
  }

  // In general the above pass will delete all the resources and there won't
  // be any left in the map. However, if parts of the implementation are still
  // holding on to internal refs, we need to tell them that the instance is
  // gone.
  to_delete = data.resources;
  cur = to_delete.begin();
  while (cur != to_delete.end()) {
    ResourceMap::iterator found_resource = live_resources_.find(*cur);
    if (found_resource != live_resources_.end())
      found_resource->second.first->NotifyInstanceWasDeleted();
    cur++;
  }

  instance_map_.erase(instance);
}

int ResourceTracker::GetLiveObjectsForInstance(PP_Instance instance) const {
  CheckThreadingPreconditions();
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return 0;
  return static_cast<int>(found->second->resources.size());
}

void ResourceTracker::UseOddResourceValueInDebugMode() {
#if !defined(NDEBUG)
  DCHECK_EQ(0, last_resource_value_);

  ++last_resource_value_;
#endif
}

PP_Resource ResourceTracker::AddResource(Resource* object) {
  CheckThreadingPreconditions();
  // If the plugin manages to create too many resources, don't do crazy stuff.
  if (last_resource_value_ >= kMaxPPId)
    return 0;

  // Allocate an ID. Note there's a rare error condition below that means we
  // could end up not using |new_id|, but that's harmless.
  PP_Resource new_id = MakeTypedId(GetNextResourceValue(), PP_ID_TYPE_RESOURCE);

  // Some objects have a 0 instance, meaning they aren't associated with any
  // instance, so they won't be in |instance_map_|. This is (as of this writing)
  // only true of the PPB_MessageLoop resource for the main thread.
  if (object->pp_instance()) {
    InstanceMap::iterator found = instance_map_.find(object->pp_instance());
    if (found == instance_map_.end()) {
      // If you hit this, it's likely somebody forgot to call DidCreateInstance,
      // the resource was created with an invalid PP_Instance, or the renderer
      // side tried to create a resource for a plugin that crashed/exited. This
      // could happen for OOP plugins where due to reentrancies in context of
      // outgoing sync calls the renderer can send events after a plugin has
      // exited.
      VLOG(1) << "Failed to find plugin instance in instance map";
      return 0;
    }
    found->second->resources.insert(new_id);
  }

  live_resources_[new_id] = ResourceAndRefCount(object, 0);
  return new_id;
}

void ResourceTracker::RemoveResource(Resource* object) {
  CheckThreadingPreconditions();
  PP_Resource pp_resource = object->pp_resource();
  InstanceMap::iterator found = instance_map_.find(object->pp_instance());
  if (found != instance_map_.end())
    found->second->resources.erase(pp_resource);
  live_resources_.erase(pp_resource);
}

void ResourceTracker::LastPluginRefWasDeleted(Resource* object) {
  // Bug http://crbug.com/134611 indicates that sometimes the resource tracker
  // is null here. This should never be the case since if we have a resource in
  // the tracker, it should always have a valid instance associated with it
  // (except for the resource for the main thread's message loop, which has
  // instance set to 0).
  // As a result, we do some CHECKs here to see what types of problems the
  // instance might have before dispatching.
  //
  // TODO(brettw) remove these checks when this bug is no longer relevant.
  // Note, we do an imperfect check here; this might be a loop that's not the
  // main one.
  const bool is_message_loop = (object->AsPPB_MessageLoop_API() != NULL);
  CHECK(object->pp_instance() || is_message_loop);
  CallbackTracker* callback_tracker =
      PpapiGlobals::Get()->GetCallbackTrackerForInstance(object->pp_instance());
  CHECK(callback_tracker || is_message_loop);
  if (callback_tracker)
    callback_tracker->PostAbortForResource(object->pp_resource());
  object->NotifyLastPluginRefWasDeleted();
}

int32_t ResourceTracker::GetNextResourceValue() {
#if defined(NDEBUG)
  return ++last_resource_value_;
#else
  // In debug mode, the least significant bit indicates which side (renderer
  // or plugin process) created the resource. Increment by 2 so it's always the
  // same.
  last_resource_value_ += 2;
  return last_resource_value_;
#endif
}

bool ResourceTracker::CanOperateOnResource(PP_Resource res) {
#if defined(NDEBUG)
  return true;
#else
  // The invalid PP_Resource value could appear at both sides.
  if (res == 0)
    return true;

  // Skipping the type bits, the least significant bit of |res| should be the
  // same as that of |last_resource_value_|.
  return ((res >> kPPIdTypeBits) & 1) == (last_resource_value_ & 1);
#endif
}

}  // namespace ppapi
