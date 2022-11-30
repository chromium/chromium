// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DISCARDABLE_MEMORY_UTILS_H_
#define CONTENT_RENDERER_DISCARDABLE_MEMORY_UTILS_H_

#include <stddef.h>

#include "base/feature_list.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/madv_free_discardable_memory_posix.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
CreateDiscardableMemoryAllocator();

}  // namespace content

#endif  // CONTENT_RENDERER_DISCARDABLE_MEMORY_UTILS_H_
