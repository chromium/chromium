// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_var_tracker.h"

#include <tuple>

#include "base/check.h"
#include "base/notreached.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/host_resource_var.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/v8object_var.h"
#include "ppapi/c/pp_var.h"

using ppapi::ArrayBufferVar;
using ppapi::V8ObjectVar;

namespace content {

HostVarTracker::V8ObjectVarKey::V8ObjectVarKey(V8ObjectVar* object_var)
    : instance(object_var->instance()->pp_instance()) {
  v8::Local<v8::Object> object = object_var->GetHandle();
  hash = object.IsEmpty() ? 0 : object->GetIdentityHash();
}

HostVarTracker::V8ObjectVarKey::V8ObjectVarKey(PP_Instance instance,
                                               v8::Local<v8::Object> object)
    : instance(instance),
      hash(object.IsEmpty() ? 0 : object->GetIdentityHash()) {}

HostVarTracker::V8ObjectVarKey::~V8ObjectVarKey() {}

bool HostVarTracker::V8ObjectVarKey::operator<(
    const V8ObjectVarKey& other) const {
  return std::tie(instance, hash) < std::tie(other.instance, other.hash);
}

HostVarTracker::HostVarTracker()
    : VarTracker(SINGLE_THREADED), last_shared_memory_map_id_(0) {}

HostVarTracker::~HostVarTracker() {}

ArrayBufferVar* HostVarTracker::CreateArrayBuffer(uint32_t size_in_bytes) {
  return new HostArrayBufferVar(size_in_bytes);
}

ArrayBufferVar* HostVarTracker::CreateShmArrayBuffer(
    uint32_t size_in_bytes,
    base::UnsafeSharedMemoryRegion region) {
  return new HostArrayBufferVar(size_in_bytes, region);
}

void HostVarTracker::AddV8ObjectVar(V8ObjectVar* object_var) {
  CheckThreadingPreconditions();
  v8::HandleScope handle_scope(object_var->instance()->GetIsolate());
  DCHECK(GetForV8Object(object_var->instance()->pp_instance(),
                        object_var->GetHandle()) == object_map_.end());
  object_map_.insert(std::make_pair(V8ObjectVarKey(object_var), object_var));
}

void HostVarTracker::RemoveV8ObjectVar(V8ObjectVar* object_var) {
  CheckThreadingPreconditions();
  v8::HandleScope handle_scope(object_var->instance()->GetIsolate());
  auto it = GetForV8Object(object_var->instance()->pp_instance(),
                           object_var->GetHandle());
  DCHECK(it != object_map_.end());
  object_map_.erase(it);
}

PP_Var HostVarTracker::V8ObjectVarForV8Object(PP_Instance instance,
                                              v8::Local<v8::Object> object) {
  CheckThreadingPreconditions();
  ObjectMap::const_iterator it = GetForV8Object(instance, object);
  if (it == object_map_.end())
    return (new V8ObjectVar(instance, object))->GetPPVar();
  return it->second->GetPPVar();
}

int HostVarTracker::GetLiveV8ObjectVarsForTest(PP_Instance instance) {
  CheckThreadingPreconditions();
  int count = 0;
  // Use a key with an empty handle to find the v8 object var in the map with
  // the given instance and the lowest hash.
  V8ObjectVarKey key(instance, v8::Local<v8::Object>());
  ObjectMap::const_iterator it = object_map_.lower_bound(key);
  while (it != object_map_.end() && it->first.instance == instance) {
    ++count;
    ++it;
  }
  return count;
}

PP_Var HostVarTracker::MakeResourcePPVarFromMessage(
    PP_Instance instance,
    const IPC::Message& creation_message,
    int pending_renderer_id,
    int pending_browser_id) {
  // On the host side, the creation message is ignored when creating a resource.
  // Therefore, a call to this function indicates a null resource. Return the
  // resource 0.
  return MakeResourcePPVar(0);
}

ppapi::ResourceVar* HostVarTracker::MakeResourceVar(PP_Resource pp_resource) {
  return new HostResourceVar(pp_resource);
}

void HostVarTracker::DidDeleteInstance(PP_Instance pp_instance) {
  CheckThreadingPreconditions();

  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  v8::HandleScope handle_scope(instance->GetIsolate());
  // Force delete all var references. ForceReleaseV8Object() will cause
  // this object, and potentially others it references, to be removed from
  // |live_vars_|.

  // Use a key with an empty handle to find the v8 object var in the map with
  // the given instance and the lowest hash.
  V8ObjectVarKey key(pp_instance, v8::Local<v8::Object>());
  auto it = object_map_.lower_bound(key);
  while (it != object_map_.end() && it->first.instance == pp_instance) {
    ForceReleaseV8Object(it->second);
    object_map_.erase(it++);
  }
}

void HostVarTracker::ForceReleaseV8Object(ppapi::V8ObjectVar* object_var) {
  object_var->InstanceDeleted();
  auto iter = live_vars_.find(object_var->GetExistingVarID());
  if (iter == live_vars_.end()) {
    NOTREACHED();
    return;
  }
  iter->second.ref_count = 0;
  DCHECK(iter->second.track_with_no_reference_count == 0);
  DeleteObjectInfoIfNecessary(iter);
}

HostVarTracker::ObjectMap::iterator HostVarTracker::GetForV8Object(
    PP_Instance instance,
    v8::Local<v8::Object> object) {
  std::pair<ObjectMap::iterator, ObjectMap::iterator> range =
      object_map_.equal_range(V8ObjectVarKey(instance, object));

  for (auto it = range.first; it != range.second; ++it) {
    if (object == it->second->GetHandle())
      return it;
  }
  return object_map_.end();
}

int HostVarTracker::TrackSharedMemoryRegion(
    PP_Instance instance,
    base::UnsafeSharedMemoryRegion region,
    uint32_t size_in_bytes) {
  SharedMemoryMapEntry entry;
  entry.instance = instance;
  entry.region = std::move(region);
  entry.size_in_bytes = size_in_bytes;

  // Find a free id for our map.
  while (shared_memory_map_.find(last_shared_memory_map_id_) !=
         shared_memory_map_.end()) {
    ++last_shared_memory_map_id_;
  }
  shared_memory_map_[last_shared_memory_map_id_] = std::move(entry);
  return last_shared_memory_map_id_;
}

bool HostVarTracker::StopTrackingSharedMemoryRegion(
    int id,
    PP_Instance instance,
    base::UnsafeSharedMemoryRegion* region,
    uint32_t* size_in_bytes) {
  auto it = shared_memory_map_.find(id);
  if (it == shared_memory_map_.end())
    return false;
  if (it->second.instance != instance)
    return false;

  *region = std::move(it->second.region);
  *size_in_bytes = it->second.size_in_bytes;
  shared_memory_map_.erase(it);
  return true;
}

}  // namespace content
