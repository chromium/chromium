// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_buffer_impl.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "content/common/pepper_file_util.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

using ppapi::thunk::PPB_Buffer_API;

namespace content {

PPB_Buffer_Impl::PPB_Buffer_Impl(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance), size_(0), map_count_(0) {}

PPB_Buffer_Impl::~PPB_Buffer_Impl() {}

// static
PP_Resource PPB_Buffer_Impl::Create(PP_Instance instance, uint32_t size) {
  scoped_refptr<PPB_Buffer_Impl> new_resource(CreateResource(instance, size));
  if (new_resource.get())
    return new_resource->GetReference();
  return 0;
}

// static
scoped_refptr<PPB_Buffer_Impl> PPB_Buffer_Impl::CreateResource(
    PP_Instance instance,
    uint32_t size) {
  scoped_refptr<PPB_Buffer_Impl> buffer(new PPB_Buffer_Impl(instance));
  if (!buffer->Init(size))
    return scoped_refptr<PPB_Buffer_Impl>();
  return buffer;
}

PPB_Buffer_Impl* PPB_Buffer_Impl::AsPPB_Buffer_Impl() { return this; }

PPB_Buffer_API* PPB_Buffer_Impl::AsPPB_Buffer_API() { return this; }

bool PPB_Buffer_Impl::Init(uint32_t size) {
  if (size == 0)
    return false;
  size_ = size;
  shared_memory_ = base::UnsafeSharedMemoryRegion::Create(size);
  return shared_memory_.IsValid();
}

PP_Bool PPB_Buffer_Impl::Describe(uint32_t* size_in_bytes) {
  *size_in_bytes = size_;
  return PP_TRUE;
}

PP_Bool PPB_Buffer_Impl::IsMapped() {
  return PP_FromBool(shared_mapping_.IsValid());
}

void* PPB_Buffer_Impl::Map() {
  DCHECK(size_);
  DCHECK(shared_memory_.IsValid());
  if (map_count_++ == 0) {
    DCHECK(!shared_mapping_.IsValid());
    shared_mapping_ = shared_memory_.Map();
  }
  return shared_mapping_.memory();
}

void PPB_Buffer_Impl::Unmap() {
  if (--map_count_ == 0)
    shared_mapping_ = {};
}

int32_t PPB_Buffer_Impl::GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) {
  *shm = &shared_memory_;
  return PP_OK;
}

BufferAutoMapper::BufferAutoMapper(PPB_Buffer_API* api) : api_(api) {
  needs_unmap_ = !PP_ToBool(api->IsMapped());
  data_ = reinterpret_cast<const uint8_t*>(api->Map());
  api->Describe(&size_);
}

BufferAutoMapper::~BufferAutoMapper() {
  if (needs_unmap_)
    api_->Unmap();
}

}  // namespace content
