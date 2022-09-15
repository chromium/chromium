// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_
#define PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class Resource;

class PPAPI_SHARED_EXPORT ResourceTracker {
 public:
  // A SINGLE_THREADED ResourceTracker will use a thread-checker to make sure
  // it's always invoked on the same thread on which it was constructed. A
  // THREAD_SAFE ResourceTracker will check that the ProxyLock is held. See
  // CheckThreadingPreconditions() for more details.
  enum ThreadMode { SINGLE_THREADED, THREAD_SAFE };
  explicit ResourceTracker(ThreadMode thread_mode);

  ResourceTracker(const ResourceTracker&) = delete;
  ResourceTracker& operator=(const ResourceTracker&) = delete;

  virtual ~ResourceTracker();

  // The returned pointer will be NULL if there is no resource. The reference
  // count of the resource is unaffected.
  Resource* GetResource(PP_Resource res) const;

  // Takes a reference on the given resource.
  // Do not call this method on on the host side for resources backed by a
  // ResourceHost.
  void AddRefResource(PP_Resource res);

  // Releases a reference on the given resource.
  // Do not call this method on on the host side for resources backed by a
  // ResourceHost.
  void ReleaseResource(PP_Resource res);

  // Notifies the tracker that a new instance has been created. This must be
  // called before creating any resources associated with the instance.
  void DidCreateInstance(PP_Instance instance);

  // Called when an instance is being deleted. All plugin refs for the
  // associated resources will be force freed, and the resources (if they still
  // exist) will be disassociated from the instance.
  void DidDeleteInstance(PP_Instance instance);

  // Returns the number of resources associated with the given instance.
  // Returns 0 if the instance isn't known.
  int GetLiveObjectsForInstance(PP_Instance instance) const;

 protected:
  // This calls AddResource and RemoveResource.
  friend class Resource;

  // On the host-side, make sure we are called on the right thread. On the
  // plugin side, make sure we have the proxy lock.
  void CheckThreadingPreconditions() const;

  // This method is called by PluginResourceTracker's constructor so that in
  // debug mode PP_Resources from the plugin process always have odd values
  // (ignoring the type bits), while PP_Resources from the renderer process have
  // even values.
  // This allows us to check that resource refs aren't added or released on the
  // wrong side.
  void UseOddResourceValueInDebugMode();

  // Adds the given resource to the tracker, associating it with the instance
  // stored in the resource object. The new resource ID is returned, and the
  // resource will have 0 plugin refcount. This is called by the resource
  // constructor.
  //
  // Returns 0 if the resource could not be added.
  virtual PP_Resource AddResource(Resource* object);

  // The opposite of AddResource, this removes the tracking information for
  // the given resource. It's called from the resource destructor.
  virtual void RemoveResource(Resource* object);

 private:
  // Calls NotifyLastPluginRefWasDeleted on the given resource object and
  // cancels pending callbacks for the resource.
  void LastPluginRefWasDeleted(Resource* object);

  int32_t GetNextResourceValue();

  // In debug mode, checks whether |res| comes from the same resource tracker.
  bool CanOperateOnResource(PP_Resource res);

  typedef std::set<PP_Resource> ResourceSet;

  struct InstanceData {
    // Lists all resources associated with the given instance as non-owning
    // pointers. This allows us to notify those resources that the instance is
    // going away (otherwise, they may crash if they outlive the instance).
    ResourceSet resources;
  };
  typedef std::unordered_map<PP_Instance, std::unique_ptr<InstanceData>>
      InstanceMap;

  InstanceMap instance_map_;

  // For each PP_Resource, keep the object pointer and a plugin use count.
  // This use count is different then Resource object's RefCount, and is
  // manipulated using this AddRefResource/UnrefResource. When the plugin use
  // count is positive, we keep an extra ref on the Resource on
  // behalf of the plugin. When it drops to 0, we free that ref, keeping
  // the resource in the list.
  //
  // A resource will be in this list as long as the object is alive.
  typedef std::pair<Resource*, int> ResourceAndRefCount;
  typedef std::unordered_map<PP_Resource, ResourceAndRefCount> ResourceMap;
  ResourceMap live_resources_;

  int32_t last_resource_value_;

  // On the host side, we want to check that we are only called on the main
  // thread. This is to protect us from accidentally using the tracker from
  // other threads (especially the IO thread). On the plugin side, the tracker
  // is protected by the proxy lock and is thread-safe, so this will be NULL.
  std::unique_ptr<base::ThreadChecker> thread_checker_;

  base::WeakPtrFactory<ResourceTracker> weak_ptr_factory_{this};
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_
