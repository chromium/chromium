// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/lazy_shared_url_loader_factory.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Tracks that an object is destroyed on the expected task runner.
class DestructionTracker : public base::RefCounted<DestructionTracker> {
 public:
  DestructionTracker(scoped_refptr<base::SequencedTaskRunner> expected_runner,
                     bool* destroyed,
                     bool* destroyed_on_correct_sequence)
      : expected_runner_(expected_runner),
        destroyed_(destroyed),
        destroyed_on_correct_sequence_(destroyed_on_correct_sequence) {}

 private:
  friend class base::RefCounted<DestructionTracker>;
  ~DestructionTracker() {
    *destroyed_ = true;
    *destroyed_on_correct_sequence_ =
        expected_runner_->RunsTasksInCurrentSequence();
  }

  scoped_refptr<base::SequencedTaskRunner> expected_runner_;
  raw_ptr<bool> destroyed_;
  raw_ptr<bool> destroyed_on_correct_sequence_;
};

// A task runner that allows controlling the return value of
// RunsTasksInCurrentSequence() for testing thread hopping.
class FakeTaskRunner : public base::SequencedTaskRunner {
 public:
  FakeTaskRunner() = default;

  void set_runs_tasks_in_current_sequence(bool runs) { runs_ = runs; }
  void set_accepts_tasks(bool accepts) { accepts_tasks_ = accepts; }

  // SequencedTaskRunner implementation:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    if (!accepts_tasks_) {
      return false;
    }
    pending_tasks_.push_back(std::move(task));
    return true;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override { return runs_; }

  bool HasPendingTask() const { return !pending_tasks_.empty(); }

  size_t NumPendingTasks() const { return pending_tasks_.size(); }

  void RunPendingTasks() {
    std::vector<base::OnceClosure> tasks;
    tasks.swap(pending_tasks_);
    for (auto& task : tasks) {
      std::move(task).Run();
    }
  }

  void CleanUpLeakedObjectsOnMainThread() {
    bool old_runs = runs_;
    // Fake running on the main thread so sequence checkers pass during
    // destruction.
    runs_ = true;
    std::vector<base::OnceClosure> tasks;
    tasks.swap(leaked_deleters_);
    for (auto& task : tasks) {
      std::move(task).Run();
    }
    runs_ = old_runs;
  }

 protected:
  bool DeleteOrReleaseSoonInternal(const base::Location& from_here,
                                   void (*deleter)(const void*),
                                   const void* object) override {
    if (!accepts_tasks_) {
      leaked_deleters_.push_back(base::BindOnce(deleter, object));
      return false;
    }
    return base::SequencedTaskRunner::DeleteOrReleaseSoonInternal(
        from_here, deleter, object);
  }

  ~FakeTaskRunner() override = default;

 private:
  friend class base::RefCountedThreadSafe<FakeTaskRunner>;

  bool runs_ = false;
  bool accepts_tasks_ = true;
  std::vector<base::OnceClosure> pending_tasks_;
  std::vector<base::OnceClosure> leaked_deleters_;
};

// A fake SharedURLLoaderFactory that records calls to CreateLoaderAndStart and
// Clone.
class FakeURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;

  // SharedURLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    create_loader_called_ = true;
    last_request_id_ = request_id;
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    clone_called_ = true;
    receivers_.Add(this, std::move(receiver));
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  bool create_loader_called() const { return create_loader_called_; }
  bool clone_called() const { return clone_called_; }
  int32_t last_request_id() const { return last_request_id_; }

  void Reset() {
    create_loader_called_ = false;
    clone_called_ = false;
  }

 private:
  friend class base::RefCountedThreadSafe<FakeURLLoaderFactory>;
  ~FakeURLLoaderFactory() override = default;

  bool create_loader_called_ = false;
  bool clone_called_ = false;
  int32_t last_request_id_ = 0;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
};

// A transport container that returns the FakeURLLoaderFactory.
class FakePendingURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit FakePendingURLLoaderFactory(
      scoped_refptr<FakeURLLoaderFactory> factory)
      : factory_(std::move(factory)) {}
  ~FakePendingURLLoaderFactory() override = default;

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override {
    return factory_;
  }

 private:
  scoped_refptr<FakeURLLoaderFactory> factory_;
};

std::unique_ptr<network::PendingSharedURLLoaderFactory>
FakeURLLoaderFactory::Clone() {
  clone_called_ = true;
  return std::make_unique<FakePendingURLLoaderFactory>(
      scoped_refptr<FakeURLLoaderFactory>(this));
}

}  // namespace

class LazySharedURLLoaderFactoryTest : public testing::Test {
 public:
  LazySharedURLLoaderFactoryTest()
      : main_thread_task_runner_(base::MakeRefCounted<FakeTaskRunner>()),
        fake_target_factory_(base::MakeRefCounted<FakeURLLoaderFactory>()) {}

  void SetUp() override { clone_callback_called_ = false; }

  void TearDown() override {
    main_thread_task_runner_->set_runs_tasks_in_current_sequence(true);
    main_thread_task_runner_->RunPendingTasks();
  }

  // The callback that will run on the "main" thread to perform the clone.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> PerformClone() {
    clone_callback_called_ = true;
    return std::make_unique<FakePendingURLLoaderFactory>(fake_target_factory_);
  }

  // Helper to create a LazySharedURLLoaderFactory wrapper.
  scoped_refptr<network::SharedURLLoaderFactory> CreateLazyFactory() {
    auto pending_lazy = std::make_unique<LazyPendingSharedURLLoaderFactory>(
        main_thread_task_runner_,
        base::BindRepeating(&LazySharedURLLoaderFactoryTest::PerformClone,
                            base::Unretained(this)));
    return network::SharedURLLoaderFactory::Create(std::move(pending_lazy));
  }

 protected:
  // Necessary to run tasks on the "worker" thread (current thread).
  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<FakeTaskRunner> main_thread_task_runner_;
  scoped_refptr<FakeURLLoaderFactory> fake_target_factory_;

  bool clone_callback_called_ = false;
};

TEST_F(LazySharedURLLoaderFactoryTest, ConstructorDoesNotClone) {
  // Creating the pending factory and instantiating the lazy factory should
  // NOT trigger the clone callback.
  auto pending_lazy = std::make_unique<LazyPendingSharedURLLoaderFactory>(
      main_thread_task_runner_,
      base::BindRepeating(&LazySharedURLLoaderFactoryTest::PerformClone,
                          base::Unretained(this)));

  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      network::SharedURLLoaderFactory::Create(std::move(pending_lazy));

  EXPECT_FALSE(clone_callback_called_);
  // In the destructor of LazyPendingSharedURLLoaderFactory, it posts a task
  // to delete clone_callback_ on the main thread runner. This is a cleanup
  // task, not a clone task, so running it should not trigger the clone
  // callback.
  main_thread_task_runner_->RunPendingTasks();
  EXPECT_FALSE(clone_callback_called_);
}

TEST_F(LazySharedURLLoaderFactoryTest, RequestTriggersCloneAndBuffers) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Make a request.
  mojo::PendingReceiver<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  lazy_factory->CreateLoaderAndStart(
      std::move(loader), 101, 0, network::ResourceRequest(), std::move(client),
      net::MutableNetworkTrafficAnnotationTag());

  // 1. Verify that the clone callback was posted to the main thread,
  //    but NOT executed yet.
  EXPECT_FALSE(clone_callback_called_);
  EXPECT_TRUE(main_thread_task_runner_->HasPendingTask());

  // 2. Verify that the request was buffered and NOT sent to the target factory
  // yet.
  EXPECT_FALSE(fake_target_factory_->create_loader_called());

  // 3. Run the clone task on the "main thread".
  main_thread_task_runner_->RunPendingTasks();
  EXPECT_TRUE(clone_callback_called_);

  // 4. At this point, the reply is posted back to the worker thread (current
  // thread),
  //    but not executed yet. The request is still buffered.
  EXPECT_FALSE(fake_target_factory_->create_loader_called());

  // 5. Run the reply task on the "worker thread" (current thread).
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->create_loader_called(); }));

  // 6. Verify that the buffered request has been flushed and sent to the target
  // factory.
  EXPECT_TRUE(fake_target_factory_->create_loader_called());
  EXPECT_EQ(fake_target_factory_->last_request_id(), 101);
}

TEST_F(LazySharedURLLoaderFactoryTest, SubsequentRequestsAreDirect) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // First request to trigger binding.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 101, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Complete binding.
  main_thread_task_runner_->RunPendingTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->create_loader_called(); }));
  EXPECT_TRUE(fake_target_factory_->create_loader_called());
  fake_target_factory_->Reset();

  // Make a second request.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 102, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Verify that the second request was sent DIRECTLY and immediately,
  // without any main thread task hops.
  EXPECT_TRUE(fake_target_factory_->create_loader_called());
  EXPECT_EQ(fake_target_factory_->last_request_id(), 102);
  EXPECT_FALSE(main_thread_task_runner_->HasPendingTask());
}

TEST_F(LazySharedURLLoaderFactoryTest, MojoCloneIsBufferedAndFlushed) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Call Mojo Clone.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> cloned_remote;
  lazy_factory->Clone(cloned_remote.InitWithNewPipeAndPassReceiver());

  // Verify it is buffered.
  EXPECT_FALSE(fake_target_factory_->clone_called());
  EXPECT_TRUE(main_thread_task_runner_->HasPendingTask());

  // Complete binding.
  main_thread_task_runner_->RunPendingTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->clone_called(); }));

  // Verify it is flushed.
  EXPECT_TRUE(fake_target_factory_->clone_called());
}

TEST_F(LazySharedURLLoaderFactoryTest, SynchronousBindingOnMainThread) {
  // Set the task runner to evaluate RunsTasksInCurrentSequence() as true.
  main_thread_task_runner_->set_runs_tasks_in_current_sequence(true);

  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Make a request.
  mojo::PendingReceiver<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  lazy_factory->CreateLoaderAndStart(
      std::move(loader), 101, 0, network::ResourceRequest(), std::move(client),
      net::MutableNetworkTrafficAnnotationTag());

  // Verify that the clone callback and actual binding occurred synchronously
  // without running any pending tasks.
  EXPECT_TRUE(clone_callback_called_);
  EXPECT_TRUE(fake_target_factory_->create_loader_called());
  EXPECT_EQ(fake_target_factory_->last_request_id(), 101);
}

TEST_F(LazySharedURLLoaderFactoryTest, DestructorThreadSafety) {
  // Simulate being on a background thread by setting main thread runner's
  // sequence check to false.
  main_thread_task_runner_->set_runs_tasks_in_current_sequence(false);

  bool callback_destroyed = false;
  bool callback_destroyed_on_correct_sequence = false;

  auto tracker = base::MakeRefCounted<DestructionTracker>(
      main_thread_task_runner_, &callback_destroyed,
      &callback_destroyed_on_correct_sequence);

  // Create a callback that captures the tracker.
  // When the callback is destroyed, the tracker is destroyed.
  auto pending_lazy = std::make_unique<LazyPendingSharedURLLoaderFactory>(
      main_thread_task_runner_,
      base::BindRepeating(
          [](scoped_refptr<DestructionTracker> tracker)
              -> std::unique_ptr<network::PendingSharedURLLoaderFactory> {
            return nullptr;
          },
          tracker));

  // 1. Instantiate the factory.
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      network::SharedURLLoaderFactory::Create(std::move(pending_lazy));

  // 2. Destroy the factory on the "worker" thread (current thread).
  lazy_factory = nullptr;
  tracker = nullptr;

  // 3. Verify that the callback was NOT destroyed yet (deferred to main
  // thread).
  EXPECT_FALSE(callback_destroyed);
  EXPECT_TRUE(main_thread_task_runner_->HasPendingTask());

  // 4. Run the cleanup task on the "main" thread.
  // Set runs_ to true so the tracker's destructor evaluates
  // RunsTasksInCurrentSequence() as true.
  main_thread_task_runner_->set_runs_tasks_in_current_sequence(true);
  main_thread_task_runner_->RunPendingTasks();

  // 5. Verify that the callback is now destroyed, and it was destroyed on the
  //    correct sequence (main thread sequence).
  EXPECT_TRUE(callback_destroyed);
  EXPECT_TRUE(callback_destroyed_on_correct_sequence);
}

TEST_F(LazySharedURLLoaderFactoryTest, MojoCloneAfterBinding) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Trigger binding with a request.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 101, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());
  main_thread_task_runner_->RunPendingTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->create_loader_called(); }));
  fake_target_factory_->Reset();

  // Call Mojo Clone *after* binding.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> cloned_remote;
  lazy_factory->Clone(cloned_remote.InitWithNewPipeAndPassReceiver());

  // Verify that it is forwarded IMMEDIATELY without any main-thread task hops.
  EXPECT_TRUE(fake_target_factory_->clone_called());
  EXPECT_FALSE(main_thread_task_runner_->HasPendingTask());
}

TEST_F(LazySharedURLLoaderFactoryTest, ObjectCloneBeforeBinding) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Clone the C++ factory object *before* binding.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_clone =
      lazy_factory->Clone();
  EXPECT_TRUE(pending_clone);

  // Instantiate the cloned factory.
  scoped_refptr<network::SharedURLLoaderFactory> cloned_factory =
      network::SharedURLLoaderFactory::Create(std::move(pending_clone));

  // Verify that triggering a request on the cloned factory triggers the clone
  // callback.
  cloned_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 102, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  EXPECT_FALSE(clone_callback_called_);
  main_thread_task_runner_->RunPendingTasks();
  EXPECT_TRUE(clone_callback_called_);
}

TEST_F(LazySharedURLLoaderFactoryTest, ObjectCloneAfterBinding) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Trigger binding.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 101, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());
  main_thread_task_runner_->RunPendingTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->create_loader_called(); }));
  fake_target_factory_->Reset();

  // Clone the C++ factory object *after* binding.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_clone =
      lazy_factory->Clone();
  EXPECT_TRUE(pending_clone);
  EXPECT_TRUE(fake_target_factory_->clone_called());
}

TEST_F(LazySharedURLLoaderFactoryTest, MultipleRequestsOnlyTriggerOneClone) {
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      CreateLazyFactory();

  // Make the first request.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 101, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Verify that exactly 2 tasks are posted to the main thread:
  // - Task 1: The cleanup task from the destruction of `pending_lazy` inside
  //           SharedURLLoaderFactory::Create().
  // - Task 2: The clone task from the first request.
  EXPECT_EQ(main_thread_task_runner_->NumPendingTasks(), 2u);

  // Make a second request *before* the first clone task finishes.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 102, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Verify that we still have exactly 2 tasks posted (early return in
  // TriggerMainThreadClone).
  EXPECT_EQ(main_thread_task_runner_->NumPendingTasks(), 2u);

  // Run the clone and reply tasks to complete binding.
  main_thread_task_runner_->RunPendingTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_target_factory_->create_loader_called(); }));

  // Verify that both requests were flushed to the target factory.
  EXPECT_TRUE(fake_target_factory_->create_loader_called());
  EXPECT_EQ(fake_target_factory_->last_request_id(), 102);

  // Verify that no additional clone tasks were posted.
  EXPECT_FALSE(main_thread_task_runner_->HasPendingTask());
}

TEST_F(LazySharedURLLoaderFactoryTest, BindFailureFailsBufferedRequests) {
  // Create a pending factory with a callback that returns nullptr.
  auto pending_lazy = std::make_unique<LazyPendingSharedURLLoaderFactory>(
      main_thread_task_runner_,
      base::BindRepeating(
          []() -> std::unique_ptr<network::PendingSharedURLLoaderFactory> {
            return nullptr;
          }));
  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      network::SharedURLLoaderFactory::Create(std::move(pending_lazy));

  // Make a request with a valid TestURLLoaderClient.
  network::TestURLLoaderClient client;
  mojo::PendingReceiver<network::mojom::URLLoader> loader;
  lazy_factory->CreateLoaderAndStart(
      std::move(loader), 101, 0, network::ResourceRequest(),
      client.CreateRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Trigger binding (which fails and returns nullptr).
  main_thread_task_runner_->RunPendingTasks();

  // Verify that the client is notified of the failure with net::ERR_FAILED.
  client.RunUntilComplete();
  EXPECT_TRUE(client.has_received_completion());
  EXPECT_EQ(client.completion_status().error_code, net::ERR_FAILED);
}

TEST_F(LazySharedURLLoaderFactoryTest, ShutdownThreadSafety) {
  // Simulate being on a background thread.
  main_thread_task_runner_->set_runs_tasks_in_current_sequence(false);

  bool callback_destroyed = false;
  bool callback_destroyed_on_correct_sequence = false;

  auto tracker = base::MakeRefCounted<DestructionTracker>(
      main_thread_task_runner_, &callback_destroyed,
      &callback_destroyed_on_correct_sequence);

  auto pending_lazy = std::make_unique<LazyPendingSharedURLLoaderFactory>(
      main_thread_task_runner_,
      base::BindRepeating(
          [](scoped_refptr<DestructionTracker> tracker)
              -> std::unique_ptr<network::PendingSharedURLLoaderFactory> {
            return nullptr;
          },
          tracker));

  scoped_refptr<network::SharedURLLoaderFactory> lazy_factory =
      network::SharedURLLoaderFactory::Create(std::move(pending_lazy));

  // The destruction of pending_lazy posts a cleanup task for its
  // clone_callback_. We must run it now before simulating shutdown so it
  // doesn't artificially keep the callback attached.
  main_thread_task_runner_->RunPendingTasks();

  // Now, simulate the main thread task runner shutting down (refusing tasks).
  main_thread_task_runner_->set_accepts_tasks(false);

  // Trigger a clone operation, which will attempt to PostTask to the main
  // thread. Because the task runner refuses the task, the task should be
  // dropped. We need to ensure that the callback is NOT destroyed on the
  // current (background) thread. It should be safely leaked.
  lazy_factory->CreateLoaderAndStart(
      mojo::NullReceiver(), 101, 0, network::ResourceRequest(),
      mojo::NullRemote(), net::MutableNetworkTrafficAnnotationTag());

  // Also destroy the factory, which triggers ReleaseCallbackOnMainThread.
  // It also attempts to post to the main thread and should be safely leaked.
  lazy_factory = nullptr;
  tracker = nullptr;

  // Verify that the callback was NOT destroyed at all (it was leaked).
  // We use DeleteSoon and it intentionally leaks objects on shutdown to avoid
  // incorrect sequence usage.
  EXPECT_FALSE(callback_destroyed);

  // Clean up leaked objects to prevent LSan failures in the test, simulating
  // that they are eventually destroyed safely or process exit cleans them up.
  main_thread_task_runner_->CleanUpLeakedObjectsOnMainThread();

  EXPECT_TRUE(callback_destroyed);
  EXPECT_TRUE(callback_destroyed_on_correct_sequence);
}

}  // namespace content
