// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/memory_dev.h"

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Memory_Dev>() {
  return PPB_MEMORY_DEV_INTERFACE;
}

}  // namespace

void* Memory_Dev::MemAlloc(uint32_t num_bytes) {
  if (!has_interface<PPB_Memory_Dev>())
    return NULL;
  return get_interface<PPB_Memory_Dev>()->MemAlloc(num_bytes);
}

void Memory_Dev::MemFree(void* ptr) {
  if (!has_interface<PPB_Memory_Dev>() || !ptr)
    return;
  get_interface<PPB_Memory_Dev>()->MemFree(ptr);
}

}  // namespace pp
