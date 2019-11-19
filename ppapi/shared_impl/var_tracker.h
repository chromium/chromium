// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VAR_TRACKER_H_
#define PPAPI_SHARED_IMPL_VAR_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_checker.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/var.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace ppapi {

class ArrayBufferVar;

// Tracks non-POD (refcounted) var objects held by a plugin.
//
// The tricky part is the concept of a "tracked object". These are only
// necessary in the plugin side of the proxy when running out of process. A
// tracked object is one that the plugin is aware of, but doesn't hold a
// reference to. This will happen when the plugin is passed an object as an
// argument from the host (renderer) as an input argument to a sync function,
// but where ownership is not passed.
//
// This class maintains the "track_with_no_reference_count" but doesn't do
// anything with it other than call virtual functions. The interesting parts
// are added by the PluginObjectVar derived from this class.
class PPAPI_SHARED_EXPORT VarTracker {
 public:
  // A SINGLE_THREADED VarTracker will use a thread-checker to make sure it's
  // always invoked on the same thread on which it was constructed. A
  // THREAD_SAFE VarTracker will check that the ProxyLock is held. See
  // CheckThreadingPreconditions() for more details.
  enum ThreadMode { SINGLE_THREADED, THREAD_SAFE };
  explicit VarTracker(ThreadMode thread_mode);
  virtual ~VarTracker();

  // Called by the Var object to add a new var to the tracker.
  int32_t AddVar(Var* var);

  // Looks up a given var and returns a reference to the Var if it exists.
  // Returns NULL if the var type is not an object we track (POD) or is
  // invalid.
  Var* GetVar(int32_t var_id) const;
  Var* GetVar(const PP_Var& var) const;

  // Increases a previously-known Var ID's refcount, returning true on success,
  // false if the ID is invalid. The PP_Var version returns true and does
  // nothing for non-refcounted type vars.
  bool AddRefVar(int32_t var_id);
  bool AddRefVar(const PP_Var& var);

  // Decreases the given Var ID's refcount, returning true on success, false if
  // the ID is invalid or if the refcount was already 0. The PP_Var version
  // returns true and does nothing for non-refcounted type vars. The var will
  // be deleted if there are no more refs to it.
  bool ReleaseVar(int32_t var_id);
  bool ReleaseVar(const PP_Var& var);

  // Create a new array buffer of size |size_in_bytes|. Return a PP_Var that
  // that references it and has an initial reference-count of 1.
  PP_Var MakeArrayBufferPPVar(uint32_t size_in_bytes);
  // Same as above, but copy the contents of |data| in to the new array buffer.
  PP_Var MakeArrayBufferPPVar(uint32_t size_in_bytes, const void* data);
  // Same as above, but copy the contents of the shared memory in |h|
  // into the new array buffer.
  PP_Var MakeArrayBufferPPVar(uint32_t size_in_bytes,
                              base::UnsafeSharedMemoryRegion h);

  // Create an ArrayBuffer and copy the contents of |data| in to it. The
  // returned object has 0 reference count in the tracker, and like all
  // RefCounted objects, has a 0 initial internal reference count. (You should
  // usually immediately put this in a scoped_refptr).
  ArrayBufferVar* MakeArrayBufferVar(uint32_t size_in_bytes, const void* data);

  // Creates a new resource var from a resource creation message. Returns a
  // PP_Var that references a new PP_Resource, both with an initial reference
  // count of 1. On the host side, |creation_message| is ignored, and an empty
  // resource var is always returned.
  virtual PP_Var MakeResourcePPVarFromMessage(
      PP_Instance instance,
      const IPC::Message& creation_message,
      int pending_renderer_id,
      int pending_browser_id) = 0;

  // Creates a new resource var that points to a given resource ID. Returns a
  // PP_Var that references it and has an initial reference count of 1.
  // If |pp_resource| is 0, returns a valid, empty resource var. On the plugin
  // side (where it is possible to tell which resources exist), if |pp_resource|
  // does not exist, returns a null var.
  PP_Var MakeResourcePPVar(PP_Resource pp_resource);

  // Creates a new resource var that points to a given resource ID. This is
  // implemented by the host and plugin tracker separately, because the plugin
  // keeps a reference to the resource, and the host does not.
  // If |pp_resource| is 0, returns a valid, empty resource var. On the plugin
  // side (where it is possible to tell which resources exist), if |pp_resource|
  // does not exist, returns NULL.
  virtual ResourceVar* MakeResourceVar(PP_Resource pp_resource) = 0;

  // Return a vector containing all PP_Vars that are in the tracker. This is to
  // help implement PPB_Testing_Private.GetLiveVars and should generally not be
  // used in production code. The PP_Vars are returned in no particular order,
  // and their reference counts are unaffected.
  std::vector<PP_Var> GetLiveVars();

  // Retrieves the internal reference counts for testing. Returns 0 if we
  // know about the object but the corresponding value is 0, or -1 if the
  // given object ID isn't in our map.
  int GetRefCountForObject(const PP_Var& object);
  int GetTrackedWithNoReferenceCountForObject(const PP_Var& object);

  // Returns true if the given vartype is refcounted and has associated objects
  // (it's not POD).
  static bool IsVarTypeRefcounted(PP_VarType type);

  // Called after an instance is deleted to do var cleanup.
  virtual void DidDeleteInstance(PP_Instance instance) = 0;

  // Returns an "id" for a shared memory region that can be safely sent between
  // the host and plugin, and resolved back into the original region on the
  // host. Not implemented on the plugin side.
  virtual int TrackSharedMemoryRegion(PP_Instance instance,
                                      base::UnsafeSharedMemoryRegion region,
                                      uint32_t size_in_bytes) = 0;

  // Resolves an "id" generated by TrackSharedMemoryHandle back into
  // a UnsafeSharedMemoryRegion and its size on the host.
  // Not implemented on the plugin side.
  virtual bool StopTrackingSharedMemoryRegion(
      int id,
      PP_Instance instance,
      base::UnsafeSharedMemoryRegion* region,
      uint32_t* size_in_bytes) = 0;

 protected:
  struct PPAPI_SHARED_EXPORT VarInfo {
    VarInfo();
    VarInfo(Var* v, int input_ref_count);

    scoped_refptr<Var> var;

    // Explicit reference count. This value is affected by the renderer calling
    // AddRef and Release. A nonzero value here is represented by a single
    // reference in the host on our behalf (this reduces IPC traffic).
    int ref_count;

    // Tracked object count (see class comment above).
    //
    // "TrackObjectWithNoReference" might be called recursively in rare cases.
    // For example, say the host calls a plugin function with an object as an
    // argument, and in response, the plugin calls a host function that then
    // calls another (or the same) plugin function with the same object.
    //
    // This value tracks the number of calls to TrackObjectWithNoReference so
    // we know when we can stop tracking this object.
    int track_with_no_reference_count;
  };
  typedef std::unordered_map<int32_t, VarInfo> VarMap;

  // Specifies what should happen with the refcount when calling AddVarInternal.
  enum AddVarRefMode {
    ADD_VAR_TAKE_ONE_REFERENCE,
    ADD_VAR_CREATE_WITH_NO_REFERENCE
  };

  // On the host-side, make sure we are called on the right thread. On the
  // plugin side, make sure we have the proxy lock.
  void CheckThreadingPreconditions() const;

  // Implementation of AddVar that allows the caller to specify whether the
  // initial refcount of the added object will be 0 or 1.
  //
  // Overridden in the plugin proxy to do additional object tracking.
  virtual int32_t AddVarInternal(Var* var, AddVarRefMode mode);

  // Convenience functions for doing lookups into the live_vars_ map.
  VarMap::iterator GetLiveVar(int32_t id);
  VarMap::iterator GetLiveVar(const PP_Var& var);
  VarMap::const_iterator GetLiveVar(const PP_Var& var) const;

  // Called when AddRefVar increases a "tracked" ProxyObject's refcount from
  // zero to one. In the plugin side of the proxy, we need to send some
  // messages to the host. In the host side, this should never be called since
  // there are no proxy objects.
  virtual void TrackedObjectGettingOneRef(VarMap::const_iterator iter);

  // Called when ReleaseVar decreases a object's refcount from one to zero. It
  // may still be "tracked" (has a "track_with_no_reference_count") value. In
  // the plugin side of the proxy, we need to tell the host that we no longer
  // have a reference. In the host side, this should never be called since
  // there are no proxy objects.
  virtual void ObjectGettingZeroRef(VarMap::iterator iter);

  // Called when an object may have had its refcount or
  // track_with_no_reference_count value decreased. If the object has neither
  // refs anymore, this will remove it and return true. Returns false if it's
  // still alive.
  //
  // Overridden by the PluginVarTracker to also clean up the host info map.
  virtual bool DeleteObjectInfoIfNecessary(VarMap::iterator iter);

  VarMap live_vars_;

  // Last assigned var ID.
  int32_t last_var_id_;

 private:
  // Create and return a new ArrayBufferVar size_in_bytes bytes long. This is
  // implemented by the Host and Plugin tracker separately, so that it can be
  // a real WebKit ArrayBuffer on the host side.
  virtual ArrayBufferVar* CreateArrayBuffer(uint32_t size_in_bytes) = 0;
  virtual ArrayBufferVar* CreateShmArrayBuffer(
      uint32_t size_in_bytes,
      base::UnsafeSharedMemoryRegion region) = 0;

  // On the host side, we want to check that we are only called on the main
  // thread. This is to protect us from accidentally using the tracker from
  // other threads (especially the IO thread). On the plugin side, the tracker
  // is protected by the proxy lock and is thread-safe, so this will be NULL.
  std::unique_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VarTracker);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VAR_TRACKER_H_
