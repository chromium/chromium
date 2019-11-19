// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_
#define CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "v8/include/v8.h"

namespace ppapi {
class ArrayBufferVar;
class V8ObjectVar;
}

namespace content {

class HostVarTracker : public ppapi::VarTracker {
 public:
  HostVarTracker();
  ~HostVarTracker() override;

  // Tracks all live V8ObjectVar. This is so we can map between instance +
  // V8Object and get the V8ObjectVar corresponding to it. This Add/Remove
  // function is called by the V8ObjectVar when it is created and destroyed.
  void AddV8ObjectVar(ppapi::V8ObjectVar* object_var);
  void RemoveV8ObjectVar(ppapi::V8ObjectVar* object_var);
  // Creates or retrieves a V8ObjectVar.
  PP_Var V8ObjectVarForV8Object(PP_Instance instance,
                                v8::Local<v8::Object> object);
  // Returns the number of V8ObjectVars associated with the given instance.
  // Returns 0 if the instance isn't known.
  CONTENT_EXPORT int GetLiveV8ObjectVarsForTest(PP_Instance instance);

  // VarTracker public implementation.
  PP_Var MakeResourcePPVarFromMessage(PP_Instance instance,
                                      const IPC::Message& creation_message,
                                      int pending_renderer_id,
                                      int pending_browser_id) override;
  ppapi::ResourceVar* MakeResourceVar(PP_Resource pp_resource) override;
  void DidDeleteInstance(PP_Instance pp_instance) override;

  int TrackSharedMemoryRegion(PP_Instance instance,
                              base::UnsafeSharedMemoryRegion region,
                              uint32_t size_in_bytes) override;
  bool StopTrackingSharedMemoryRegion(int id,
                                      PP_Instance instance,
                                      base::UnsafeSharedMemoryRegion* region,
                                      uint32_t* size_in_bytes) override;

 private:
  // VarTracker private implementation.
  ppapi::ArrayBufferVar* CreateArrayBuffer(uint32_t size_in_bytes) override;
  ppapi::ArrayBufferVar* CreateShmArrayBuffer(
      uint32_t size_in_bytes,
      base::UnsafeSharedMemoryRegion region) override;

  // Clear the reference count of the given object and remove it from
  // live_vars_.
  void ForceReleaseV8Object(ppapi::V8ObjectVar* object_var);

  // A non-unique, ordered key for a V8ObjectVar. Contains the hash of the v8
  // and the instance it is associated with.
  struct V8ObjectVarKey {
    explicit V8ObjectVarKey(ppapi::V8ObjectVar* object_var);
    V8ObjectVarKey(PP_Instance i, v8::Local<v8::Object> object);
    ~V8ObjectVarKey();

    bool operator<(const V8ObjectVarKey& other) const;

    PP_Instance instance;
    int hash;
  };
  typedef std::multimap<V8ObjectVarKey, ppapi::V8ObjectVar*> ObjectMap;

  // Returns an iterator into |object_map| which points to V8Object which
  // is associated with the given instance and object.
  ObjectMap::iterator GetForV8Object(PP_Instance instance,
                                     v8::Local<v8::Object> object);


  // A multimap of V8ObjectVarKey -> ObjectMap.
  ObjectMap object_map_;

  // Tracks all shared memory handles used for transmitting array buffers.
  struct SharedMemoryMapEntry {
    PP_Instance instance;
    base::UnsafeSharedMemoryRegion region;
    uint32_t size_in_bytes;
  };
  typedef std::map<int, SharedMemoryMapEntry> SharedMemoryMap;
  SharedMemoryMap shared_memory_map_;
  uint32_t last_shared_memory_map_id_;

  DISALLOW_COPY_AND_ASSIGN(HostVarTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_HOST_VAR_TRACKER_H_
