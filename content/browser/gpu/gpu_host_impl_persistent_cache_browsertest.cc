// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "components/viz/common/features.h"
#include "components/viz/host/persistent_cache_sandboxed_file_factory.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "components/viz/test/stub_gpu_service.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "skia/buildflags.h"

namespace content {

namespace {

// Test implementation of viz::mojom::GpuService which only implements the
// persistent cache aspects.
class TestGpuService : public viz::StubGpuService {
 public:
  explicit TestGpuService(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}
  TestGpuService(const TestGpuService*) = delete;
  ~TestGpuService() override = default;
  TestGpuService& operator=(const TestGpuService&) = delete;

  // mojom::GpuService:
  void SetChannelPersistentCachePendingBackend(
      int32_t client_id,
      const gpu::GpuDiskCacheHandle& handle,
      persistent_cache::PendingBackend pending_backend) override {
    quit_closure_.Run();
  }

 private:
  base::RepeatingClosure quit_closure_;
};

class TestCacheFileFactory : public viz::PersistentCacheSandboxedFileFactory {
 public:
  TestCacheFileFactory(
      const base::FilePath& cache_root_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : viz::PersistentCacheSandboxedFileFactory(
            cache_root_dir,
            std::move(background_task_runner)) {}

 private:
  ~TestCacheFileFactory() override = default;
};

}  // namespace

// Test GpuHostImpl's behaviors when PersistentCache is used for shader caching.
class GpuHostImplPersistentCacheTest : public ContentBrowserTest {
 public:
  GpuHostImplPersistentCacheTest()
      : gpu_service_thread_("Gpu Service"),
        cache_factory_thread_("Cache Factory") {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSkiaGraphite,
          {{features::kSkiaGraphiteDawnUsePersistentCache.name, "true"}}}},
        {});

    CHECK(temp_dir_.CreateUniqueTempDir());
    cache_factory_thread_.Start();
    cache_factory_ = base::MakeRefCounted<TestCacheFileFactory>(
        temp_dir_.GetPath(), cache_factory_thread_.task_runner());
    viz::PersistentCacheSandboxedFileFactory::SetInstanceForTesting(
        cache_factory_.get());
  }
  ~GpuHostImplPersistentCacheTest() override {
    viz::PersistentCacheSandboxedFileFactory::SetInstanceForTesting(nullptr);
    cache_factory_thread_.Stop();
  }

  void PreRunTestOnMainThread() override {
    gpu_service_thread_.StartAndWaitForTesting();

    run_loop_for_cache_file_wait_ = std::make_unique<base::RunLoop>();

    gpu_host_impl_test_api_ = std::make_unique<viz::GpuHostImplTestApi>(
        GpuProcessHost::Get()->gpu_host());
    mojo::Remote<viz::mojom::GpuService> gpu_service_remote;
    auto receiver = gpu_service_remote.BindNewPipeAndPassReceiver();
    gpu_host_impl_test_api_->SetGpuService(std::move(gpu_service_remote));

    PostTaskToGpuServiceThreadAndWait(
        base::BindOnce(&GpuHostImplPersistentCacheTest::InitOnGpuServiceThread,
                       base::Unretained(this), std::move(receiver),
                       run_loop_for_cache_file_wait_->QuitClosure()));

    ContentBrowserTest::PreRunTestOnMainThread();
  }

  void PostRunTestOnMainThread() override {
    PostTaskToGpuServiceThreadAndWait(
        base::BindOnce([](GpuServiceThreadState gpu_service_thread_state) {},
                       std::move(gpu_service_thread_state_)));
    gpu_service_thread_.Stop();

    ContentBrowserTest::PostRunTestOnMainThread();
  }

  void WaitForSetChannelPersistentCacheFile() {
    run_loop_for_cache_file_wait_->Run();
  }

 protected:
  void InitOnGpuServiceThread(
      mojo::PendingReceiver<viz::mojom::GpuService> receiver,
      base::RepeatingClosure quit_closure) {
    gpu_service_thread_state_.test_gpu_service =
        std::make_unique<TestGpuService>(std::move(quit_closure));
    gpu_service_thread_state_.gpu_service_receiver =
        std::make_unique<mojo::Receiver<viz::mojom::GpuService>>(
            gpu_service_thread_state_.test_gpu_service.get(),
            std::move(receiver));
  }

  void PostTaskToGpuServiceThreadAndWait(base::OnceClosure task) {
    base::WaitableEvent completion_event;

    gpu_service_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure task, base::WaitableEvent* wait_event) {
              std::move(task).Run();
              wait_event->Signal();
            },
            std::move(task), &completion_event));

    completion_event.Wait();
  }

  struct GpuServiceThreadState {
    std::unique_ptr<TestGpuService> test_gpu_service;
    std::unique_ptr<mojo::Receiver<viz::mojom::GpuService>>
        gpu_service_receiver;
  };

  base::test::ScopedFeatureList feature_list_;
  base::Thread gpu_service_thread_;
  base::Thread cache_factory_thread_;
  // RunLoop for waiting on the main thread for SetChannelPersistentCacheFile
  // to trigger.
  std::unique_ptr<base::RunLoop> run_loop_for_cache_file_wait_;
  std::unique_ptr<viz::GpuHostImplTestApi> gpu_host_impl_test_api_;
  GpuServiceThreadState gpu_service_thread_state_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<TestCacheFileFactory> cache_factory_;
};

#if BUILDFLAG(SKIA_USE_DAWN)
// Check that the cache files exist after SetChannelPersistentCacheFile is
// called.
IN_PROC_BROWSER_TEST_F(GpuHostImplPersistentCacheTest,
                       SetChannelPersistentCacheFileCalled) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  WaitForSetChannelPersistentCacheFile();

  EXPECT_TRUE(base::PathExists(temp_dir_.GetPath()));
  EXPECT_FALSE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

// Check that the cache files are cleared after a crash.
IN_PROC_BROWSER_TEST_F(GpuHostImplPersistentCacheTest, ClearCacheOnCrash) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  WaitForSetChannelPersistentCacheFile();

  // Get the cache directory. It should be the only child folder in the temp
  // dir.
  base::FileEnumerator enumerator(temp_dir_.GetPath(), false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath cache_dir = enumerator.Next();
  EXPECT_FALSE(cache_dir.empty());
  EXPECT_TRUE(enumerator.Next().empty());
  // Verify that the cache dir is not empty.
  EXPECT_FALSE(base::IsDirectoryEmpty(cache_dir));

  base::RunLoop run_loop;
  // Watch for cache directory changes
  base::FilePathWatcher watcher;
  watcher.Watch(cache_dir, base::FilePathWatcher::Type::kNonRecursive,
                base::BindRepeating(
                    [](base::RunLoop* run_loop, const base::FilePath& cache_dir,
                       const base::FilePath& path, bool error) {
                      if (base::IsDirectoryEmpty(cache_dir)) {
                        run_loop->Quit();
                      }
                    },
                    &run_loop, cache_dir));

  // Simulate a crash.
  viz::GpuHostImpl* gpu_host_impl = GpuProcessHost::Get()->gpu_host();
  gpu::GpuProcessShmCount service_count(
      gpu_host_impl->GetShaderCacheShmCountForTesting()->CloneRegion());
  gpu::GpuProcessShmCount::ScopedIncrement scoped_increment(&service_count);

  gpu_host_impl->OnProcessCrashed();

  // The cache directory should be empty after the crash.
  run_loop.Run();

  EXPECT_TRUE(base::IsDirectoryEmpty(cache_dir));
}

#endif  // BUILDFLAG(SKIA_USE_DAWN)

}  // namespace content
