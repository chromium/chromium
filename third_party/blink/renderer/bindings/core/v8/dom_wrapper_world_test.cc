// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

Vector<scoped_refptr<DOMWrapperWorld>> CreateIsolatedWorlds(
    v8::Isolate* isolate) {
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  worlds.push_back(DOMWrapperWorld::EnsureIsolatedWorld(
      isolate, DOMWrapperWorld::kMainWorldId + 1));
  worlds.push_back(DOMWrapperWorld::EnsureIsolatedWorld(
      isolate, DOMWrapperWorld::kDOMWrapperWorldEmbedderWorldIdLimit - 1));
  EXPECT_TRUE(worlds[0]->IsIsolatedWorld());
  EXPECT_TRUE(worlds[1]->IsIsolatedWorld());
  return worlds;
}

Vector<scoped_refptr<DOMWrapperWorld>> CreateWorlds(v8::Isolate* isolate) {
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  worlds.push_back(
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker));
  worlds.push_back(
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker));
  worlds.push_back(
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker));
  EXPECT_TRUE(worlds[0]->IsWorkerWorld());
  EXPECT_TRUE(worlds[1]->IsWorkerWorld());
  EXPECT_TRUE(worlds[2]->IsWorkerWorld());

  // World ids should be unique.
  HashSet<int32_t> world_ids;
  EXPECT_TRUE(world_ids.insert(worlds[0]->GetWorldId()).is_new_entry);
  EXPECT_TRUE(world_ids.insert(worlds[1]->GetWorldId()).is_new_entry);
  EXPECT_TRUE(world_ids.insert(worlds[2]->GetWorldId()).is_new_entry);

  return worlds;
}

void WorkerThreadFunc(
    WorkerBackingThread* thread,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  thread->InitializeOnBackingThread(
      WorkerBackingThreadStartupData::CreateDefault());

  // Worlds on the main thread should not be visible from the worker thread.
  Vector<scoped_refptr<DOMWrapperWorld>> retrieved_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_TRUE(retrieved_worlds.IsEmpty());

  // Create worlds on the worker thread and verify them.
  Vector<scoped_refptr<DOMWrapperWorld>> worlds =
      CreateWorlds(thread->GetIsolate());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(worlds.size(), retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of remaining worlds.
  for (scoped_refptr<DOMWrapperWorld>& world : worlds) {
    if (world->IsWorkerWorld())
      world->Dispose();
  }
  worlds.clear();

  thread->ShutdownOnBackingThread();
  PostCrossThreadTask(*main_thread_task_runner, FROM_HERE,
                      CrossThreadBindOnce(&test::ExitRunLoop));
}

TEST(DOMWrapperWorldTest, Basic) {
  // Create the main world and verify it.
  DOMWrapperWorld& main_world = DOMWrapperWorld::MainWorld();
  EXPECT_TRUE(main_world.IsMainWorld());
  EXPECT_FALSE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  Vector<scoped_refptr<DOMWrapperWorld>> retrieved_worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(1u, retrieved_worlds.size());
  EXPECT_TRUE(retrieved_worlds[0]->IsMainWorld());
  retrieved_worlds.clear();

  // Create isolated worlds and verify them.
  V8TestingScope scope;
  Vector<scoped_refptr<DOMWrapperWorld>> isolated_worlds =
      CreateIsolatedWorlds(scope.GetIsolate());
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + 1, retrieved_worlds.size());

  // Create other worlds and verify them.
  Vector<scoped_refptr<DOMWrapperWorld>> worlds =
      CreateWorlds(scope.GetIsolate());
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  retrieved_worlds.clear();
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + worlds.size() + 1,
            retrieved_worlds.size());
  retrieved_worlds.clear();

  // Start a worker thread and create worlds on that.
  std::unique_ptr<WorkerBackingThread> thread =
      std::make_unique<WorkerBackingThread>(
          ThreadCreationParams(ThreadType::kTestThread)
              .SetThreadNameForTest("DOMWrapperWorld test thread"));
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      Thread::Current()->GetTaskRunner();
  PostCrossThreadTask(*thread->BackingThread().GetTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(&WorkerThreadFunc,
                                          CrossThreadUnretained(thread.get()),
                                          std::move(main_thread_task_runner)));
  test::EnterRunLoop();

  // Worlds on the worker thread should not be visible from the main thread.
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(isolated_worlds.size() + worlds.size() + 1,
            retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of the isolated worlds.
  isolated_worlds.clear();
  EXPECT_TRUE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(worlds.size() + 1, retrieved_worlds.size());
  retrieved_worlds.clear();

  // Dispose of the other worlds.
  for (scoped_refptr<DOMWrapperWorld>& world : worlds) {
    if (world->IsWorkerWorld())
      world->Dispose();
  }
  worlds.clear();
  EXPECT_FALSE(DOMWrapperWorld::NonMainWorldsExistInMainThread());
  DOMWrapperWorld::AllWorldsInCurrentThread(retrieved_worlds);
  EXPECT_EQ(1u, retrieved_worlds.size());
}

}  // namespace
}  // namespace blink
