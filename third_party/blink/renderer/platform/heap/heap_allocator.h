// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_

// Legacy: Users including `heap_allocator.h` actually expect it to pull in the
// containers.
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_list_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_
