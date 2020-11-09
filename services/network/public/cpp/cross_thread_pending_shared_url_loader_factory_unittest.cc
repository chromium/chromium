// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

const char kUrl[] = "http://www.example.com/";
const char kData[] = "payload!";

// Checks to make sure clone method is called on the right thread.
class CloneCheckingURLLoaderFactory : public TestURLLoaderFactory {
 public:
  explicit CloneCheckingURLLoaderFactory(
      scoped_refptr<base::SequencedTaskRunner> owning_thread)
      : owning_thread_(owning_thread) {}

  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
    EXPECT_TRUE(owning_thread_->RunsTasksInCurrentSequence());
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> owning_thread_;
};

}  // namespace

// Base class with shared setup logic.
class CrossThreadPendingSharedURLLoaderFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    main_thread_ = base::SequencedTaskRunnerHandle::Get();
    loader_thread_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives()});

    test_url_loader_factory_ =
        std::make_unique<CloneCheckingURLLoaderFactory>(loader_thread_);
    test_url_loader_factory_->SetInterceptor(base::BindRepeating(
        &CrossThreadPendingSharedURLLoaderFactoryTest::CheckLoaderThread,
        base::Unretained(this)));
    test_url_loader_factory_->AddResponse(kUrl, kData);

    shared_factory_ = base::MakeRefCounted<WeakWrapperSharedURLLoaderFactory>(
        test_url_loader_factory_.get());

    base::RunLoop run_loop;
    loader_thread_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CrossThreadPendingSharedURLLoaderFactoryTest::
                           SetupFactoryInfoOnLoaderThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();

    // Release |shared_factory_| on |loader_thread_|
    base::RunLoop run_loop;
    loader_thread_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CrossThreadPendingSharedURLLoaderFactoryTest::
                           ReleaseSharedFactoryOnLoaderThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void CheckLoaderThread(const ResourceRequest& request) {
    EXPECT_TRUE(loader_thread_->RunsTasksInCurrentSequence());
  }

  void SetupFactoryInfoOnLoaderThread() {
    DCHECK(loader_thread_->RunsTasksInCurrentSequence());
    pending_factory_ =
        std::make_unique<CrossThreadPendingSharedURLLoaderFactory>(
            shared_factory_);
  }

  void ReleaseSharedFactoryOnLoaderThread() {
    DCHECK(loader_thread_->RunsTasksInCurrentSequence());
    shared_factory_ = nullptr;
  }

  void TestLoad(scoped_refptr<SharedURLLoaderFactory> factory,
                scoped_refptr<base::SequencedTaskRunner> client_runner,
                base::OnceClosure quit_closure) {
    // Make sure we can fetch through |factory|, and that things are on proper
    // threads (this is partly done by the CheckLoaderThread interceptor).
    auto request = std::make_unique<ResourceRequest>();
    request->url = GURL(kUrl);

    std::unique_ptr<SimpleURLLoader> loader = SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);
    SimpleURLLoader* loader_raw = loader.get();
    loader_raw->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory.get(),
        base::BindOnce(
            [](scoped_refptr<base::SequencedTaskRunner> client_runner,
               base::OnceClosure quit_closure,
               std::unique_ptr<SimpleURLLoader> loader,
               std::unique_ptr<std::string> result) {
              EXPECT_TRUE(client_runner->RunsTasksInCurrentSequence());
              if (!result) {
                ADD_FAILURE();
              } else {
                EXPECT_EQ(kData, *result);
              }
              std::move(quit_closure).Run();
            },
            client_runner, std::move(quit_closure), std::move(loader)));
  }

  void TestClone(scoped_refptr<SharedURLLoaderFactory> factory) {
    mojo::PendingRemote<mojom::URLLoaderFactory> factory_client;
    factory->Clone(factory_client.InitWithNewPipeAndPassReceiver());
  }

  void TestLoadOnMainThread(scoped_refptr<SharedURLLoaderFactory> factory) {
    base::RunLoop run_loop;
    TestLoad(factory, main_thread_, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<CloneCheckingURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<SharedURLLoaderFactory> shared_factory_;

  scoped_refptr<base::SequencedTaskRunner> loader_thread_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_;

  std::unique_ptr<CrossThreadPendingSharedURLLoaderFactory> pending_factory_;
};

TEST_F(CrossThreadPendingSharedURLLoaderFactoryTest, Basic) {
  // Test with things created from |pending_factory_|.
  scoped_refptr<SharedURLLoaderFactory> main_thread_factory =
      SharedURLLoaderFactory::Create(std::move(pending_factory_));
  TestLoadOnMainThread(main_thread_factory);
  TestClone(main_thread_factory);
}

TEST_F(CrossThreadPendingSharedURLLoaderFactoryTest, FurtherClone) {
  // Test load with result of a further clone.
  scoped_refptr<SharedURLLoaderFactory> main_thread_factory =
      SharedURLLoaderFactory::Create(std::move(pending_factory_));
  scoped_refptr<SharedURLLoaderFactory> main_thread_factory_clone =
      SharedURLLoaderFactory::Create(main_thread_factory->Clone());

  TestLoadOnMainThread(main_thread_factory_clone);
  TestLoadOnMainThread(main_thread_factory);
  TestClone(main_thread_factory_clone);
  TestClone(main_thread_factory);
}

TEST_F(CrossThreadPendingSharedURLLoaderFactoryTest, CloneThirdThread) {
  // Clone to a third thread.
  scoped_refptr<base::SequencedTaskRunner> third_thread =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()});

  scoped_refptr<SharedURLLoaderFactory> main_thread_factory =
      SharedURLLoaderFactory::Create(std::move(pending_factory_));
  std::unique_ptr<PendingSharedURLLoaderFactory> new_pending_factory =
      main_thread_factory->Clone();

  base::RunLoop run_loop;
  base::OnceClosure run_loop_quit = run_loop.QuitClosure();
  third_thread->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scoped_refptr<SharedURLLoaderFactory> third_thread_factory =
            SharedURLLoaderFactory::Create(std::move(new_pending_factory));
        TestLoad(third_thread_factory, third_thread, std::move(run_loop_quit));
        TestClone(third_thread_factory);
      }));
  run_loop.Run();
}

TEST_F(CrossThreadPendingSharedURLLoaderFactoryTest, CloneLoaderThread) {
  // Clone back to the loader thread.
  scoped_refptr<SharedURLLoaderFactory> main_thread_factory =
      SharedURLLoaderFactory::Create(std::move(pending_factory_));
  std::unique_ptr<PendingSharedURLLoaderFactory> new_pending_factory =
      main_thread_factory->Clone();

  base::RunLoop run_loop;
  base::OnceClosure run_loop_quit = run_loop.QuitClosure();
  loader_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scoped_refptr<SharedURLLoaderFactory> load_thread_factory =
            SharedURLLoaderFactory::Create(std::move(new_pending_factory));
        TestLoad(load_thread_factory, loader_thread_, std::move(run_loop_quit));
        TestClone(load_thread_factory);
      }));
  run_loop.Run();
}

}  // namespace network
