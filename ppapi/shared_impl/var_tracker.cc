// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/var_tracker.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

VarTracker::VarInfo::VarInfo()
    : var(), ref_count(0), track_with_no_reference_count(0) {}

VarTracker::VarInfo::VarInfo(Var* v, int input_ref_count)
    : var(v), ref_count(input_ref_count), track_with_no_reference_count(0) {}

VarTracker::VarTracker(ThreadMode thread_mode) : last_var_id_(0) {
  if (thread_mode == SINGLE_THREADED)
    thread_checker_ = std::make_unique<base::ThreadChecker>();
}

VarTracker::~VarTracker() {}

void VarTracker::CheckThreadingPreconditions() const {
  DCHECK(!thread_checker_ || thread_checker_->CalledOnValidThread());
#ifndef NDEBUG
  ProxyLock::AssertAcquired();
#endif
}

int32_t VarTracker::AddVar(Var* var) {
  CheckThreadingPreconditions();

  return AddVarInternal(var, ADD_VAR_TAKE_ONE_REFERENCE);
}

Var* VarTracker::GetVar(int32_t var_id) const {
  CheckThreadingPreconditions();

  VarMap::const_iterator result = live_vars_.find(var_id);
  if (result == live_vars_.end())
    return NULL;
  return result->second.var.get();
}

Var* VarTracker::GetVar(const PP_Var& var) const {
  CheckThreadingPreconditions();

  if (!IsVarTypeRefcounted(var.type))
    return NULL;
  return GetVar(static_cast<int32_t>(var.value.as_id));
}

bool VarTracker::AddRefVar(int32_t var_id) {
  CheckThreadingPreconditions();

  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator found = live_vars_.find(var_id);
  if (found == live_vars_.end()) {
    NOTREACHED();  // Invalid var.
  }

  VarInfo& info = found->second;
  if (info.ref_count == 0) {
    // All live vars with no refcount should be tracked objects.
    DCHECK(info.track_with_no_reference_count > 0);
    DCHECK(info.var->GetType() == PP_VARTYPE_OBJECT);

    TrackedObjectGettingOneRef(found);
  }

  // Basic refcount increment.
  info.ref_count++;
  CHECK(info.ref_count != std::numeric_limits<decltype(info.ref_count)>::max());
  return true;
}

bool VarTracker::AddRefVar(const PP_Var& var) {
  CheckThreadingPreconditions();

  if (!IsVarTypeRefcounted(var.type))
    return true;
  return AddRefVar(static_cast<int32_t>(var.value.as_id));
}

bool VarTracker::ReleaseVar(int32_t var_id) {
  CheckThreadingPreconditions();

  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator found = live_vars_.find(var_id);
  if (found == live_vars_.end())
    return false;

  VarInfo& info = found->second;
  if (info.ref_count == 0) {
    NOTREACHED() << "Releasing an object with zero ref";
  }
  info.ref_count--;

  if (info.ref_count == 0) {
    // Hold a reference to the Var until it is erased so that we don't re-enter
    // live_vars_.erase() during deletion.
    // TODO(raymes): Make deletion of Vars iterative instead of recursive.
    scoped_refptr<Var> var(info.var);
    if (var->GetType() == PP_VARTYPE_OBJECT) {
      // Objects have special requirements and may not necessarily be released
      // when the refcount goes to 0.
      ObjectGettingZeroRef(found);
    } else {
      // All other var types can just be released.
      DCHECK(info.track_with_no_reference_count == 0);
      var->ResetVarID();
      live_vars_.erase(found);
    }
  }
  return true;
}

bool VarTracker::ReleaseVar(const PP_Var& var) {
  CheckThreadingPreconditions();

  if (!IsVarTypeRefcounted(var.type))
    return false;
  return ReleaseVar(static_cast<int32_t>(var.value.as_id));
}

int32_t VarTracker::AddVarInternal(Var* var, AddVarRefMode mode) {
  // If the plugin manages to create millions of strings.
  if (last_var_id_ == std::numeric_limits<int32_t>::max() >> kPPIdTypeBits)
    return 0;

  int32_t new_id = MakeTypedId(++last_var_id_, PP_ID_TYPE_VAR);
  std::pair<VarMap::iterator, bool> was_inserted =
      live_vars_.insert(std::make_pair(
          new_id, VarInfo(var, mode == ADD_VAR_TAKE_ONE_REFERENCE ? 1 : 0)));
  // We should never insert an ID that already exists.
  DCHECK(was_inserted.second);

  return new_id;
}

VarTracker::VarMap::iterator VarTracker::GetLiveVar(int32_t id) {
  return live_vars_.find(id);
}

int VarTracker::GetRefCountForObject(const PP_Var& plugin_object) {
  CheckThreadingPreconditions();

  VarMap::iterator found = GetLiveVar(plugin_object);
  if (found == live_vars_.end())
    return -1;
  return found->second.ref_count;
}

int VarTracker::GetTrackedWithNoReferenceCountForObject(
    const PP_Var& plugin_object) {
  CheckThreadingPreconditions();

  VarMap::iterator found = GetLiveVar(plugin_object);
  if (found == live_vars_.end())
    return -1;
  return found->second.track_with_no_reference_count;
}

// static
bool VarTracker::IsVarTypeRefcounted(PP_VarType type) {
  return type >= PP_VARTYPE_STRING;
}

VarTracker::VarMap::iterator VarTracker::GetLiveVar(const PP_Var& var) {
  return live_vars_.find(static_cast<int32_t>(var.value.as_id));
}

VarTracker::VarMap::const_iterator VarTracker::GetLiveVar(const PP_Var& var)
    const {
  return live_vars_.find(static_cast<int32_t>(var.value.as_id));
}

PP_Var VarTracker::MakeArrayBufferPPVar(uint32_t size_in_bytes) {
  CheckThreadingPreconditions();

  scoped_refptr<ArrayBufferVar> array_buffer(CreateArrayBuffer(size_in_bytes));
  if (!array_buffer.get())
    return PP_MakeNull();
  return array_buffer->GetPPVar();
}

PP_Var VarTracker::MakeArrayBufferPPVar(uint32_t size_in_bytes,
                                        const void* data) {
  CheckThreadingPreconditions();

  ArrayBufferVar* array_buffer = MakeArrayBufferVar(size_in_bytes, data);
  return array_buffer ? array_buffer->GetPPVar() : PP_MakeNull();
}

ArrayBufferVar* VarTracker::MakeArrayBufferVar(uint32_t size_in_bytes,
                                               const void* data) {
  CheckThreadingPreconditions();

  ArrayBufferVar* array_buffer(CreateArrayBuffer(size_in_bytes));
  if (!array_buffer)
    return nullptr;
  std::copy_n(static_cast<const uint8_t*>(data), size_in_bytes,
              static_cast<uint8_t*>(array_buffer->Map()));
  return array_buffer;
}

PP_Var VarTracker::MakeArrayBufferPPVar(uint32_t size_in_bytes,
                                        base::UnsafeSharedMemoryRegion region) {
  CheckThreadingPreconditions();

  scoped_refptr<ArrayBufferVar> array_buffer(
      CreateShmArrayBuffer(size_in_bytes, std::move(region)));
  if (!array_buffer.get())
    return PP_MakeNull();
  return array_buffer->GetPPVar();
}

PP_Var VarTracker::MakeResourcePPVar(PP_Resource pp_resource) {
  CheckThreadingPreconditions();

  ResourceVar* resource_var = MakeResourceVar(pp_resource);
  return resource_var ? resource_var->GetPPVar() : PP_MakeNull();
}

std::vector<PP_Var> VarTracker::GetLiveVars() {
  CheckThreadingPreconditions();

  std::vector<PP_Var> var_vector;
  var_vector.reserve(live_vars_.size());
  for (VarMap::const_iterator iter = live_vars_.begin();
       iter != live_vars_.end();
       ++iter) {
    var_vector.push_back(iter->second.var->GetPPVar());
  }
  return var_vector;
}

void VarTracker::TrackedObjectGettingOneRef(VarMap::const_iterator obj) {
  // Anybody using tracked objects should override this.
  NOTREACHED();
}

void VarTracker::ObjectGettingZeroRef(VarMap::iterator iter) {
  DeleteObjectInfoIfNecessary(iter);
}

bool VarTracker::DeleteObjectInfoIfNecessary(VarMap::iterator iter) {
  if (iter->second.ref_count != 0 ||
      iter->second.track_with_no_reference_count != 0)
    return false;  // Object still alive.
  iter->second.var->ResetVarID();
  live_vars_.erase(iter);
  return true;
}

}  // namespace ppapi
