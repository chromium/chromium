// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace {

// Collects the worlds present and the last used isolated world id.
std::pair<Persistent<HeapVector<Member<DOMWrapperWorld>>>, int32_t>
CollectInitialWorlds(v8::Isolate* isolate) {
  auto* initial_worlds =
      MakeGarbageCollected<HeapVector<Member<DOMWrapperWorld>>>();
  int32_t used_isolated_world_id = DOMWrapperWorld::kMainWorldId;
  DOMWrapperWorld::AllWorldsInIsolate(isolate, *initial_worlds);
  for (const auto& world : *initial_worlds) {
    if (world->IsIsolatedWorld()) {
      used_isolated_world_id =
          std::max(used_isolated_world_id, world->GetWorldId());
    }
  }
  return {initial_worlds, used_isolated_world_id};
}

auto NumberOfWorlds(v8::Isolate* isolate) {
  HeapVector<Member<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInIsolate(isolate, worlds);
  const auto num_worlds = worlds.size();
  worlds.clear();
  return num_worlds;
}

}  // namespace

TEST(DOMWrapperWorldTest, MainWorld) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  DOMWrapperWorld& main_world = DOMWrapperWorld::MainWorld(isolate);
  EXPECT_TRUE(main_world.IsMainWorld());
  EXPECT_EQ(main_world.GetWorldId(), DOMWrapperWorld::kMainWorldId);
}

TEST(DOMWrapperWorldTest, IsolatedWorlds) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  const auto [initial_worlds, used_isolated_world_id] =
      CollectInitialWorlds(isolate);
  ASSERT_TRUE(DOMWrapperWorld::IsIsolatedWorldId(used_isolated_world_id + 1));

  const auto* isolated_world1 =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, used_isolated_world_id + 1);
  const auto* isolated_world2 =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, used_isolated_world_id + 2);
  EXPECT_TRUE(isolated_world1->IsIsolatedWorld());
  EXPECT_TRUE(isolated_world2->IsIsolatedWorld());
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());

  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size() + 2);
  // Remove temporary worlds via stackless GC.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size());
}

TEST(DOMWrapperWorldTest, ExplicitDispose) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  const auto [initial_worlds, used_isolated_world_id] =
      CollectInitialWorlds(isolate);
  ASSERT_TRUE(DOMWrapperWorld::IsIsolatedWorldId(used_isolated_world_id + 1));

  auto* worker_world1 = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kWorkerOrWorklet);
  auto* worker_world2 = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kWorkerOrWorklet);
  auto* worker_world3 = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kWorkerOrWorklet);
  EXPECT_TRUE(worker_world1->IsWorkerOrWorkletWorld());
  EXPECT_TRUE(worker_world2->IsWorkerOrWorkletWorld());
  EXPECT_TRUE(worker_world3->IsWorkerOrWorkletWorld());
  HashSet<int32_t> worker_world_ids;
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world1->GetWorldId()).is_new_entry);
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world2->GetWorldId()).is_new_entry);
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world3->GetWorldId()).is_new_entry);
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());

  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size() + 3);
  // Explicitly disposing worlds will clear internal state but not remove them.
  worker_world1->Dispose();
  worker_world2->Dispose();
  worker_world3->Dispose();
  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size() + 3);
  // GC will remove the worlds.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size());
}

namespace {

void WorkerThreadFunc(
    WorkerBackingThread* thread,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    CrossThreadOnceClosure quit_closure) {
  thread->InitializeOnBackingThread(
      WorkerBackingThreadStartupData::CreateDefault());

  v8::Isolate* isolate = thread->GetIsolate();
  // Worlds on the main thread should not be visible from the worker thread.
  HeapVector<Member<DOMWrapperWorld>> initial_worlds;
  DOMWrapperWorld::AllWorldsInIsolate(isolate, initial_worlds);
  EXPECT_TRUE(initial_worlds.empty());

  // Create worlds on the worker thread and verify them.
  auto* worker_world1 = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kWorkerOrWorklet);
  auto* worker_world2 = DOMWrapperWorld::Create(
      isolate, DOMWrapperWorld::WorldType::kWorkerOrWorklet);
  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds.size() + 2);

  // Dispose of remaining worlds.
  worker_world1->Dispose();
  worker_world2->Dispose();

  thread->ShutdownOnBackingThread();
  PostCrossThreadTask(*main_thread_task_runner, FROM_HERE,
                      CrossThreadBindOnce(std::move(quit_closure)));
}

}  // namespace

TEST(DOMWrapperWorldTest, NonMainThreadWorlds) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  const auto [initial_worlds, used_isolated_world_id] =
      CollectInitialWorlds(isolate);
  ASSERT_TRUE(DOMWrapperWorld::IsIsolatedWorldId(used_isolated_world_id + 1));

  base::RunLoop loop;
  // Start a worker thread and create worlds on that.
  std::unique_ptr<WorkerBackingThread> thread =
      std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(ThreadType::kTestThread)
              .SetThreadNameForTest("DOMWrapperWorld test thread"));
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      blink::scheduler::GetSingleThreadTaskRunnerForTesting();
  PostCrossThreadTask(
      *thread->BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerThreadFunc,
                          CrossThreadUnretained(thread.get()),
                          std::move(main_thread_task_runner),
                          CrossThreadOnceClosure(loop.QuitClosure())));
  loop.Run();

  // Worlds on the worker thread should not be visible from the main thread.
  EXPECT_EQ(NumberOfWorlds(isolate), initial_worlds->size());
}

}  // namespace blink
