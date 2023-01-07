// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

#include <algorithm>

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace {

void WorkerThreadFunc(
    WorkerBackingThread* thread,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  thread->InitializeOnBackingThread(
      WorkerBackingThreadStartupData::CreateDefault());

  // Worlds on the main thread should not be visible from the worker thread.
  Vector<scoped_refptr<DOMWrapperWorld>> initial_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(initial_worlds);
  EXPECT_TRUE(initial_worlds.empty());

  // Create worlds on the worker thread and verify them.
  v8::Isolate* isolate = thread->GetIsolate();
  auto worker_world1 =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
  auto worker_world2 =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size() + 2);
  worlds.clear();

  // Dispose of remaining worlds.
  worker_world1->Dispose();
  worker_world2->Dispose();
  worker_world1.reset();
  worker_world2.reset();

  thread->ShutdownOnBackingThread();
  PostCrossThreadTask(*main_thread_task_runner, FROM_HERE,
                      CrossThreadBindOnce(&test::ExitRunLoop));
}

TEST(DOMWrapperWorldTest, Basic) {
  // Initial setup
  DOMWrapperWorld& main_world = DOMWrapperWorld::MainWorld();
  EXPECT_TRUE(main_world.IsMainWorld());
  Vector<scoped_refptr<DOMWrapperWorld>> initial_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(initial_worlds);
  int32_t used_isolated_world_id = DOMWrapperWorld::kMainWorldId;
  for (const auto& world : initial_worlds) {
    if (world->IsIsolatedWorld()) {
      used_isolated_world_id =
          std::max(used_isolated_world_id, world->GetWorldId());
    }
  }
  ASSERT_TRUE(DOMWrapperWorld::IsIsolatedWorldId(used_isolated_world_id + 1));
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Isolated worlds
  auto isolated_world1 =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, used_isolated_world_id + 1);
  auto isolated_world2 =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, used_isolated_world_id + 2);
  EXPECT_TRUE(isolated_world1->IsIsolatedWorld());
  EXPECT_TRUE(isolated_world2->IsIsolatedWorld());
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size() + 2);
  worlds.clear();
  isolated_world1.reset();
  isolated_world2.reset();
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size());
  worlds.clear();

  // Worker worlds
  auto worker_world1 =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
  auto worker_world2 =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
  auto worker_world3 =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
  EXPECT_TRUE(worker_world1->IsWorkerWorld());
  EXPECT_TRUE(worker_world2->IsWorkerWorld());
  EXPECT_TRUE(worker_world3->IsWorkerWorld());
  HashSet<int32_t> worker_world_ids;
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world1->GetWorldId()).is_new_entry);
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world2->GetWorldId()).is_new_entry);
  EXPECT_TRUE(
      worker_world_ids.insert(worker_world3->GetWorldId()).is_new_entry);
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size() + 3);
  worlds.clear();
  worker_world1->Dispose();
  worker_world2->Dispose();
  worker_world3->Dispose();
  worker_world1.reset();
  worker_world2.reset();
  worker_world3.reset();
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size());
  worlds.clear();

  // Start a worker thread and create worlds on that.
  std::unique_ptr<WorkerBackingThread> thread =
      std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(ThreadType::kTestThread)
              .SetThreadNameForTest("DOMWrapperWorld test thread"));
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      blink::scheduler::GetSingleThreadTaskRunnerForTesting();
  PostCrossThreadTask(*thread->BackingThread().GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&WorkerThreadFunc,
                                          CrossThreadUnretained(thread.get()),
                                          std::move(main_thread_task_runner)));
  test::EnterRunLoop();

  // Worlds on the worker thread should not be visible from the main thread.
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  EXPECT_EQ(worlds.size(), initial_worlds.size());
  worlds.clear();
}

}  // namespace
}  // namespace blink
