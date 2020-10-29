// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

namespace {

struct BackingModifier {
  bool can_modify;
  BasePage* const page;
  HeapObjectHeader* const header;
};

BackingModifier CanModifyBacking(ThreadState* const state, void* address) {
  // - |SweepForbidden| protects against modifying objects from destructors.
  // - |IsSweepingInProgress| protects against modifying objects while
  // concurrent sweeping is in progress.
  // - |in_atomic_pause| protects against modifying objects from within the GC.
  // This can
  //   e.g. happen when hash table buckets that have containers inlined are
  //   freed during weakness processing.
  // - |IsMarkingInProgress| protects against incremental marking which may have
  //   registered callbacks.
  if (state->SweepForbidden() || state->IsSweepingInProgress() ||
      state->in_atomic_pause() || state->IsMarkingInProgress())
    return {false, nullptr, nullptr};

  // - Don't adjust large objects because their page is never reused.
  // - Don't free backings allocated on other threads.
  BasePage* page = PageFromObject(address);
  if (page->IsLargeObjectPage() || page->Arena()->GetThreadState() != state)
    return {false, nullptr, nullptr};

  HeapObjectHeader* const header = HeapObjectHeader::FromPayload(address);
  // - Guards against pages that have not been swept. Technically, it should be
  // fine to modify those backings. We bail out to maintain the invariant that
  // no marked backing is modified.
  if (header->IsMarked())
    return {false, nullptr, nullptr};
  return {true, page, header};
}

}  // namespace

void HeapAllocator::BackingFree(void* address) {
  if (!address)
    return;

  ThreadState* const state = ThreadState::Current();
  BackingModifier result = CanModifyBacking(state, address);
  if (!result.can_modify)
    return;

  static_cast<NormalPage*>(result.page)
      ->ArenaForNormalPage()
      ->PromptlyFreeObject(result.header);
}

void HeapAllocator::FreeVectorBacking(void* address) {
  BackingFree(address);
}

void HeapAllocator::FreeHashTableBacking(void* address) {
  BackingFree(address);
}

bool HeapAllocator::BackingExpand(void* address, size_t new_size) {
  if (!address)
    return false;

  ThreadState* state = ThreadState::Current();

  BackingModifier result = CanModifyBacking(state, address);
  if (!result.can_modify)
    return false;
  DCHECK(!state->in_atomic_pause());
  DCHECK(state->IsAllocationAllowed());
  DCHECK_EQ(&state->Heap(), &ThreadState::FromObject(address)->Heap());

  // FIXME: Support expand for large objects.
  // Don't expand backings allocated on other threads.
  BasePage* page = PageFromObject(address);
  if (page->IsLargeObjectPage() || page->Arena()->GetThreadState() != state)
    return false;

  HeapObjectHeader* header = HeapObjectHeader::FromPayload(address);
  NormalPageArena* arena = static_cast<NormalPage*>(page)->ArenaForNormalPage();
  return arena->ExpandObject(header, new_size);
}

bool HeapAllocator::ExpandVectorBacking(void* address, size_t new_size) {
  return BackingExpand(address, new_size);
}

bool HeapAllocator::ExpandHashTableBacking(void* address, size_t new_size) {
  return BackingExpand(address, new_size);
}

bool HeapAllocator::BackingShrink(void* address,
                                  size_t quantized_current_size,
                                  size_t quantized_shrunk_size) {
  if (!address || quantized_shrunk_size == quantized_current_size)
    return true;

  DCHECK_LT(quantized_shrunk_size, quantized_current_size);

  ThreadState* const state = ThreadState::Current();
  BackingModifier result = CanModifyBacking(state, address);
  if (!result.can_modify)
    return false;

  DCHECK(state->IsAllocationAllowed());
  DCHECK_EQ(&state->Heap(), &ThreadState::FromObject(address)->Heap());

  NormalPageArena* arena =
      static_cast<NormalPage*>(result.page)->ArenaForNormalPage();
  // We shrink the object only if the shrinking will make a non-small
  // prompt-free block.
  // FIXME: Optimize the threshold size.
  if (quantized_current_size <= quantized_shrunk_size +
                                    sizeof(HeapObjectHeader) +
                                    sizeof(void*) * 32 &&
      !arena->IsObjectAllocatedAtAllocationPoint(result.header))
    return true;

  arena->ShrinkObject(result.header, quantized_shrunk_size);
  return true;
}

bool HeapAllocator::ShrinkVectorBacking(void* address,
                                        size_t quantized_current_size,
                                        size_t quantized_shrunk_size) {
  return BackingShrink(address, quantized_current_size, quantized_shrunk_size);
}

}  // namespace blink
