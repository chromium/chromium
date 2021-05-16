// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"

#include "gin/public/v8_platform.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/custom_spaces.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8.h"
#include "v8/include/cppgc/heap-consistency.h"
#include "v8/include/v8-cppgc.h"

namespace blink {

namespace {

// Handler allowing for dropping V8 wrapper objects that can be recreated
// lazily.
class BlinkRootsHandler final : public v8::EmbedderRootsHandler {
 public:
  explicit BlinkRootsHandler(v8::CppHeap& cpp_heap) : cpp_heap_(cpp_heap) {}
  ~BlinkRootsHandler() final = default;

  bool IsRoot(const v8::TracedReference<v8::Value>& handle) final {
    const uint16_t class_id = handle.WrapperClassId();
    // Stand-alone reference or kCustomWrappableId. Keep as root as
    // we don't know better.
    if (class_id != WrapperTypeInfo::kNodeClassId &&
        class_id != WrapperTypeInfo::kObjectClassId)
      return true;

    const v8::TracedReference<v8::Object>& traced =
        handle.template As<v8::Object>();
    if (ToWrapperTypeInfo(traced)->IsActiveScriptWrappable() &&
        ToScriptWrappable(traced)->HasPendingActivity()) {
      return true;
    }

    if (ToScriptWrappable(traced)->HasEventListeners()) {
      return true;
    }

    return false;
  }

  bool IsRoot(const v8::TracedGlobal<v8::Value>& handle) final {
    CHECK(false) << "Blink does not use v8::TracedGlobal.";
    return false;
  }

  // ResetRoot() clears references to V8 wrapper objects in all worlds. It is
  // invoked for references where IsRoot() returned false during young
  // generation garbage collections.
  void ResetRoot(const v8::TracedReference<v8::Value>& handle) final {
    const uint16_t class_id = handle.WrapperClassId();
    // Only consider handles that have not been treated as roots, see IsRoot().
    if (class_id != WrapperTypeInfo::kNodeClassId &&
        class_id != WrapperTypeInfo::kObjectClassId)
      return;

    // Clearing the wrapper below adjusts the DOM wrapper store which may
    // re-allocate its backing. NoGarbageCollectionScope is required to avoid
    // triggering a GC from such re-allocating calls as ResetRoot() is itself
    // called from GC.
    cppgc::subtle::NoGarbageCollectionScope no_gc(cpp_heap_.GetHeapHandle());
    const v8::TracedReference<v8::Object>& traced = handle.As<v8::Object>();
    bool success = DOMWrapperWorld::UnsetSpecificWrapperIfSet(
        ToScriptWrappable(traced), traced);
    // Since V8 found a handle, Blink needs to find it as well when trying to
    // remove it.
    CHECK(success);
  }

 private:
  v8::CppHeap& cpp_heap_;
};

}  // namespace

// static
base::LazyInstance<WTF::ThreadSpecific<ThreadState*>>::Leaky
    ThreadState::thread_specific_ = LAZY_INSTANCE_INITIALIZER;

// static
alignas(ThreadState) uint8_t
    ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

// static
ThreadState* ThreadState::AttachMainThread() {
  return new (main_thread_state_storage_) ThreadState(gin::V8Platform::Get());
}

// static
ThreadState* ThreadState::AttachMainThreadForTesting(v8::Platform* platform) {
  ThreadState* thread_state =
      new (main_thread_state_storage_) ThreadState(platform);
  thread_state->EnableDetachedGarbageCollectionsForTesting();
  return thread_state;
}

// static
ThreadState* ThreadState::AttachCurrentThread() {
  return new ThreadState(gin::V8Platform::Get());
}

// static
ThreadState* ThreadState::AttachCurrentThreadForTesting(
    v8::Platform* platform) {
  ThreadState* thread_state = new ThreadState(platform);
  thread_state->EnableDetachedGarbageCollectionsForTesting();
  return thread_state;
}

// static
void ThreadState::DetachCurrentThread() {
  auto* state = ThreadState::Current();
  DCHECK(state);
  delete state;
}

void ThreadState::AttachToIsolate(v8::Isolate* isolate,
                                  V8BuildEmbedderGraphCallback) {
  isolate->AttachCppHeap(cpp_heap_.get());
  CHECK_EQ(cpp_heap_.get(), isolate->GetCppHeap());
  isolate_ = isolate;
  embedder_roots_handler_ = std::make_unique<BlinkRootsHandler>(cpp_heap());
  isolate_->SetEmbedderRootsHandler(embedder_roots_handler_.get());
}

void ThreadState::DetachFromIsolate() {
  CHECK_EQ(cpp_heap_.get(), isolate_->GetCppHeap());
  isolate_->DetachCppHeap();
  isolate_->SetEmbedderRootsHandler(nullptr);
  isolate_ = nullptr;
}

ThreadState::ThreadState(v8::Platform* platform)
    : cpp_heap_(v8::CppHeap::Create(
          platform,
          {CustomSpaces::CreateCustomSpaces(),
           v8::WrapperDescriptor(kV8DOMWrapperTypeIndex,
                                 kV8DOMWrapperObjectIndex,
                                 gin::GinEmbedder::kEmbedderBlink)})),
      allocation_handle_(cpp_heap_->GetAllocationHandle()),
      heap_handle_(cpp_heap_->GetHeapHandle()),
      thread_id_(CurrentThread()) {
  *(thread_specific_.Get()) = this;
}

ThreadState::~ThreadState() {
  DCHECK(!IsMainThread());
  DCHECK(IsCreationThread());
  cpp_heap_->Terminate();
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(IsCreationThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  if (forced_scheduled_gc_for_testing_) {
    CollectAllGarbageForTesting(stack_state);
    forced_scheduled_gc_for_testing_ = false;
  }
}

void ThreadState::NotifyGarbageCollection(v8::GCType type,
                                          v8::GCCallbackFlags flags) {
  if (flags & v8::kGCCallbackFlagForced) {
    // Forces a precise GC at the end of the current event loop. This is
    // required for testing code that cannot use GC internals but rather has
    // to rely on window.gc(). Only schedule additional GCs if the last GC was
    // using conservative stack scanning.
    if (type == v8::kGCTypeScavenge) {
      forced_scheduled_gc_for_testing_ = true;
    } else if (type == v8::kGCTypeMarkSweepCompact) {
      forced_scheduled_gc_for_testing_ =
          cppgc::subtle::HeapState::PreviousGCWasConservative(heap_handle());
    }
  }
}

void ThreadState::CollectAllGarbageForTesting(BlinkGC::StackState stack_state) {
  size_t previous_live_bytes = 0;
  for (size_t i = 0; i < 5; i++) {
    // Either triggers unified heap or stand-alone garbage collections.
    cpp_heap().CollectGarbageForTesting(
        stack_state == BlinkGC::kHeapPointersOnStack
            ? cppgc::EmbedderStackState::kMayContainHeapPointers
            : cppgc::EmbedderStackState::kNoHeapPointers);
    const size_t live_bytes =
        cpp_heap()
            .CollectStatistics(cppgc::HeapStatistics::kBrief)
            .used_size_bytes;
    if (previous_live_bytes == live_bytes) {
      break;
    }
    previous_live_bytes = live_bytes;
  }
}

namespace {

class CustomSpaceStatisticsReceiverImpl final
    : public v8::CustomSpaceStatisticsReceiver {
 public:
  explicit CustomSpaceStatisticsReceiverImpl(
      base::OnceCallback<void(size_t allocated_node_bytes,
                              size_t allocated_css_bytes)> callback)
      : callback_(std::move(callback)) {}

  ~CustomSpaceStatisticsReceiverImpl() final {
    DCHECK(node_bytes_.has_value());
    DCHECK(css_bytes_.has_value());
    std::move(callback_).Run(*node_bytes_, *css_bytes_);
  }

  void AllocatedBytes(cppgc::CustomSpaceIndex space_index, size_t bytes) final {
    if (space_index.value == NodeSpace::kSpaceIndex.value) {
      node_bytes_ = bytes;
    } else {
      DCHECK_EQ(space_index.value, CSSValueSpace::kSpaceIndex.value);
      css_bytes_ = bytes;
    }
  }

 private:
  base::OnceCallback<void(size_t allocated_node_bytes,
                          size_t allocated_css_bytes)>
      callback_;
  absl::optional<size_t> node_bytes_;
  absl::optional<size_t> css_bytes_;
};

}  // anonymous namespace

void ThreadState::CollectNodeAndCssStatistics(
    base::OnceCallback<void(size_t allocated_node_bytes,
                            size_t allocated_css_bytes)> callback) {
  std::vector<cppgc::CustomSpaceIndex> spaces{NodeSpace::kSpaceIndex,
                                              CSSValueSpace::kSpaceIndex};
  cpp_heap().CollectCustomSpaceStatisticsAtLastGC(
      std::move(spaces),
      std::make_unique<CustomSpaceStatisticsReceiverImpl>(std::move(callback)));
}

void ThreadState::EnableDetachedGarbageCollectionsForTesting() {
  cpp_heap().EnableDetachedGarbageCollectionsForTesting();
  // Detached GCs cannot rely on the V8 platform being initialized which is
  // needed by cppgc to perform a garbage collection.
  static bool v8_platform_initialized = false;
  if (!v8_platform_initialized) {
    v8::V8::InitializePlatform(gin::V8Platform::Get());
    v8_platform_initialized = true;
  }
}

bool ThreadState::IsIncrementalMarking() {
  return cppgc::subtle::HeapState::IsMarking(
             ThreadState::Current()->heap_handle()) &&
         !cppgc::subtle::HeapState::IsInAtomicPause(
             ThreadState::Current()->heap_handle());
}
}  // namespace blink
