// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

struct PPP_Class_Deprecated;

namespace ppapi {

class ProxyObjectVar;

namespace proxy {

class PluginDispatcher;

// Tracks live strings and objects in the plugin process.
class PPAPI_PROXY_EXPORT PluginVarTracker : public VarTracker {
 public:
  PluginVarTracker();
  ~PluginVarTracker() override;

  // Manages tracking for receiving a VARTYPE_OBJECT from the remote side
  // (either the plugin or the renderer) that has already had its reference
  // count incremented on behalf of the caller.
  PP_Var ReceiveObjectPassRef(const PP_Var& var, PluginDispatcher* dispatcher);

  // See the comment in var_tracker.h for more about what a tracked object is.
  // This adds and releases the "track_with_no_reference_count" for a given
  // object.
  PP_Var TrackObjectWithNoReference(const PP_Var& host_var,
                                    PluginDispatcher* dispatcher);
  void StopTrackingObjectWithNoReference(const PP_Var& plugin_var);

  // Returns the host var for the corresponding plugin object var. The object
  // should be a VARTYPE_OBJECT. The reference count is not affeceted.
  PP_Var GetHostObject(const PP_Var& plugin_object) const;

  PluginDispatcher* DispatcherForPluginObject(
      const PP_Var& plugin_object) const;

  // Like Release() but the var is identified by its host object ID (as
  // returned by GetHostObject).
  void ReleaseHostObject(PluginDispatcher* dispatcher,
                         const PP_Var& host_object);

  // VarTracker public overrides.
  PP_Var MakeResourcePPVarFromMessage(PP_Instance instance,
                                      const IPC::Message& creation_message,
                                      int pending_renderer_id,
                                      int pending_browser_id) override;
  ResourceVar* MakeResourceVar(PP_Resource pp_resource) override;
  void DidDeleteInstance(PP_Instance instance) override;
  int TrackSharedMemoryRegion(PP_Instance instance,
                              base::UnsafeSharedMemoryRegion region,
                              uint32_t size_in_bytes) override;
  bool StopTrackingSharedMemoryRegion(int id,
                                      PP_Instance instance,
                                      base::UnsafeSharedMemoryRegion* region,
                                      uint32_t* size_in_bytes) override;

  // Notification that a plugin-implemented object (PPP_Class) was created by
  // the plugin or deallocated by WebKit over IPC.
  void PluginImplementedObjectCreated(PP_Instance instance,
                                      const PP_Var& created_var,
                                      const PPP_Class_Deprecated* ppp_class,
                                      void* ppp_class_data);
  void PluginImplementedObjectDestroyed(void* ppp_class_data);

  // Returns true if there is an object implemented by the plugin with the
  // given user_data that has not been deallocated yet. Call this when
  // receiving a scripting call to the plugin to validate that the object
  // receiving the call is still alive (see user_data_to_plugin_ below).
  bool IsPluginImplementedObjectAlive(void* user_data);

  // Validates that the given class/user_data pair corresponds to a currently
  // living plugin object.
  bool ValidatePluginObjectCall(const PPP_Class_Deprecated* ppp_class,
                                void* user_data);

  void DidDeleteDispatcher(PluginDispatcher* dispatcher);

 private:
  // VarTracker protected overrides.
  int32_t AddVarInternal(Var* var, AddVarRefMode mode) override;
  void TrackedObjectGettingOneRef(VarMap::const_iterator iter) override;
  void ObjectGettingZeroRef(VarMap::iterator iter) override;
  bool DeleteObjectInfoIfNecessary(VarMap::iterator iter) override;
  ArrayBufferVar* CreateArrayBuffer(uint32_t size_in_bytes) override;
  ArrayBufferVar* CreateShmArrayBuffer(
      uint32_t size_in_bytes,
      base::UnsafeSharedMemoryRegion region) override;

 private:
  friend struct base::DefaultSingletonTraits<PluginVarTracker>;
  friend class PluginProxyTestHarness;

  // Represents a var as received from the host.
  struct HostVar {
    HostVar(PluginDispatcher* d, int32_t i);

    bool operator<(const HostVar& other) const;

    // The dispatcher that sent us this object. This is used so we know how to
    // send back requests on this object.
    PluginDispatcher* dispatcher;

    // The object ID that the host generated to identify the object. This is
    // unique only within that host: different hosts could give us different
    // objects with the same ID.
    int32_t host_object_id;
  };

  struct PluginImplementedVar {
    const PPP_Class_Deprecated* ppp_class;

    // The instance that created this Var. This will be 0 if the instance has
    // been destroyed but the object is still alive.
    PP_Instance instance;

    // Represents the plugin var ID for the var corresponding to this object.
    // If the plugin does not have a ref to the object but it's still alive
    // (the DOM could be holding a ref keeping it alive) this will be 0.
    //
    // There is an obscure corner case. If the plugin returns an object to the
    // renderer and releases all of its refs, the object will still be alive
    // but there will be no plugin refs. It's possible for the plugin to get
    // this same object again through the DOM, and we'll lose the correlation
    // between plugin implemented object and car. This means we won't know when
    // the plugin releases its last refs and may call Deallocate when the
    // plugin is still holding a ref.
    //
    // However, for the plugin to be depending on holding a ref to an object
    // that it implements that it previously released but got again through
    // indirect means would be extremely rare, and we only allow var scripting
    // in limited cases anyway.
    int32_t plugin_object_id;
  };

  // Returns the existing var ID for the given object var, creating and
  // assigning an ID to it if necessary. This does not affect the reference
  // count, so in the creation case the refcount will be 0. It's assumed in
  // this case the caller will either adjust the refcount or the
  // track_with_no_reference_count.
  PP_Var GetOrCreateObjectVarID(ProxyObjectVar* object);

  // Sends an addref or release message to the browser for the given object ID.
  void SendAddRefObjectMsg(const ProxyObjectVar& proxy_object);
  void SendReleaseObjectMsg(const ProxyObjectVar& proxy_object);

  // Looks up the given host var. If we already know about it, returns a
  // reference to the already-tracked object. If it doesn't creates a new one
  // and returns it. If it's created, it's not added to the map.
  scoped_refptr<ProxyObjectVar> FindOrMakePluginVarFromHostVar(
      const PP_Var& var,
      PluginDispatcher* dispatcher);

  // Maps host vars in the host to IDs in the plugin process.
  typedef std::map<HostVar, int32_t> HostVarToPluginVarMap;
  HostVarToPluginVarMap host_var_to_plugin_var_;

  // Maps "user data" for plugin implemented objects (PPP_Class) that are
  // alive to various tracking info.
  //
  // This is tricky because there may not actually be any vars in the plugin
  // associated with a plugin-implemented object, so they won't all have
  // entries in our HostVarToPluginVarMap or the base class VarTracker's map.
  //
  // All objects that the plugin has created using CreateObject that have not
  // yet been Deallocate()-ed by WebKit will be in this map. When the instance
  // that created the object goes away, we know to call Deallocate on all
  // remaining objects for that instance so that the data backing the object
  // that the plugin owns is not leaked. We may not receive normal Deallocate
  // calls from WebKit because the object could be leaked (attached to the DOM
  // and outliving the plugin instance) or WebKit could send the deallocate
  // after the out-of-process routing for that instance was torn down.
  //
  // There is an additional complexity. In WebKit, objects created by the
  // plugin aren't actually bound to the plugin instance (for example, you
  // could attach it to the DOM or send it to another plugin instance). It's
  // possible that we could force deallocate an object when an instance id
  // destroyed, but then another instance could get to that object somehow
  // (like by reading it out of the DOM). We will then have deallocated the
  // object and can't complete the call. We do not care about this case, and
  // the calls will just fail.
  typedef std::map<void*, PluginImplementedVar>
      UserDataToPluginImplementedVarMap;
  UserDataToPluginImplementedVarMap user_data_to_plugin_;

  DISALLOW_COPY_AND_ASSIGN(PluginVarTracker);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_
