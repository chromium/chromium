// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/gpu.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

class TestGpuImpl : public mojom::Gpu {
 public:
  TestGpuImpl() = default;

  TestGpuImpl(const TestGpuImpl&) = delete;
  TestGpuImpl& operator=(const TestGpuImpl&) = delete;

  ~TestGpuImpl() override = default;

  void SetRequestWillSucceed(bool request_will_succeed) {
    request_will_succeed_ = request_will_succeed;
  }

  void CloseBindingOnRequest() { close_binding_on_request_ = true; }

  void BindReceiver(mojo::PendingReceiver<mojom::Gpu> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void EstablishGpuChannel(EstablishGpuChannelCallback callback) override {
    if (close_binding_on_request_) {
      // Don't run |callback| and trigger a connection error on the other end.
      receivers_.Clear();
      return;
    }

    constexpr int client_id = 1;
    mojo::ScopedMessagePipeHandle handle;
    if (request_will_succeed_) {
      mojo::MessagePipe message_pipe;
      handle = std::move(message_pipe.handle0);
      gpu_channel_handle_ = std::move(message_pipe.handle1);
    }

    std::move(callback).Run(client_id, std::move(handle), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            gpu::SharedImageCapabilities());
  }

  void BindGpuChannel(mojo::ScopedMessagePipeHandle handle) {
    channel_receivers_.Add(
        &mock_gpu_channel_,
        mojo::PendingReceiver<gpu::mojom::GpuChannel>(std::move(handle)));
  }

#if BUILDFLAG(IS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override {}
#endif  // BUILDFLAG(IS_CHROMEOS)

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override {}

  void CloseChannelBindings() { channel_receivers_.Clear(); }

  gpu::MockGpuChannel mock_gpu_channel_;

 private:
  bool request_will_succeed_ = true;
  bool close_binding_on_request_ = false;
  mojo::ReceiverSet<mojom::Gpu> receivers_;
  // Mocks the GPU-process side of the established GPU channels.
  mojo::ReceiverSet<gpu::mojom::GpuChannel> channel_receivers_;

  // Closing this handle will result in GpuChannelHost being lost.
  mojo::ScopedMessagePipeHandle gpu_channel_handle_;
};

}  // namespace

class GpuTest : public testing::Test {
 public:
  GpuTest() : io_thread_("GPUIOThread") {
    base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
    thread_options.thread_type = base::ThreadType::kDefault;
    CHECK(io_thread_.StartWithOptions(std::move(thread_options)));
  }

  GpuTest(const GpuTest&) = delete;
  GpuTest& operator=(const GpuTest&) = delete;

  ~GpuTest() override = default;

  Gpu* gpu() { return gpu_.get(); }

  TestGpuImpl* gpu_impl() { return gpu_impl_.get(); }

  base::Thread& io_thread() { return io_thread_; }

  // Runs all tasks posted to IO thread and any tasks posted back to main thread
  // as a result.
  void FlushMainAndIO() {
    base::RunLoop run_loop;
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure callback) { std::move(callback).Run(); },
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Runs all tasks posted to IO thread and blocks main thread.
  void BlockMainFlushIO() {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* event) {
              base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed)
                  .RunUntilIdle();
              event->Signal();
            },
            base::Unretained(&event)));
    event.Wait();
  }

  void DestroyGpu() { gpu_.reset(); }

  // testing::Test:
  void SetUp() override {
    gpu_impl_ = std::make_unique<TestGpuImpl>();
    gpu_ = base::WrapUnique(new Gpu(GetRemote(), io_thread_.task_runner(),
                                    /*client_id=*/0,
                                    mojo::ScopedMessagePipeHandle(),
                                    base::NullCallback()));
  }

  void TearDown() override {
    FlushMainAndIO();
    DestroyGpuImplOnIO();
  }

 protected:
  mojo::PendingRemote<mojom::Gpu> GetRemote() {
    mojo::PendingRemote<mojom::Gpu> remote;
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TestGpuImpl::BindReceiver,
                                  base::Unretained(gpu_impl_.get()),
                                  remote.InitWithNewPipeAndPassReceiver()));
    return remote;
  }

  void DestroyGpuImplOnIO() {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<TestGpuImpl> gpu_impl,
                          base::WaitableEvent* event) {
                         gpu_impl.reset();
                         event->Signal();
                       },
                       std::move(gpu_impl_), &event));
    event.Wait();
  }

  // Declared here to ensure it outlives `env_` and `io_thread_`. This prevents
  // crashes during teardown where background threads might access features
  // while `ScopedFeatureList` is being destroyed in derived classes.
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment env_;
  base::Thread io_thread_;
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<TestGpuImpl> gpu_impl_;
};

// Tests that multiple calls for establishing a gpu channel are all notified
// correctly when the channel is established (or fails to establish).
TEST_F(GpuTest, EstablishRequestsQueued) {
  int counter = 2;
  base::RunLoop run_loop;
  // A callback that decrements the counter, and runs the callback when the
  // counter reaches 0.
  auto callback = base::BindRepeating(
      [](int* counter, base::OnceClosure callback,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        EXPECT_TRUE(channel);
        --(*counter);
        if (*counter == 0)
          std::move(callback).Run();
      },
      &counter, run_loop.QuitClosure());
  gpu()->EstablishGpuChannel(callback);
  gpu()->EstablishGpuChannel(callback);
  EXPECT_EQ(2, counter);
  run_loop.Run();
  EXPECT_EQ(0, counter);
}

// Tests that multiple calls for establishing a gpu channel record metrics
// correctly.
TEST_F(GpuTest, EstablishRequestsMetrics) {
  base::HistogramTester histogram_tester;
  int counter = 2;
  base::RunLoop run_loop;
  auto callback = base::BindRepeating(
      [](int* counter, base::OnceClosure callback,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        EXPECT_TRUE(channel);
        --(*counter);
        if (*counter == 0) {
          std::move(callback).Run();
        }
      },
      &counter, run_loop.QuitClosure());
  gpu()->EstablishGpuChannel(callback);
  gpu()->EstablishGpuChannel(callback);
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "GPU.EstablishGpuChannel.EarliestRequestLatency", 1);
  histogram_tester.ExpectTotalCount(
      "GPU.EstablishGpuChannel.IndividualRequestLatency", 2);
}

// Tests that a new request for establishing a gpu channel from a callback of a
// previous callback is processed correctly.
TEST_F(GpuTest, EstablishRequestOnFailureOnPreviousRequest) {
  // Make the first request fail.
  gpu_impl()->SetRequestWillSucceed(false);

  base::RunLoop run_loop;
  scoped_refptr<gpu::GpuChannelHost> host;
  auto callback = base::BindOnce(
      [](scoped_refptr<gpu::GpuChannelHost>* out_host,
         base::OnceClosure callback, scoped_refptr<gpu::GpuChannelHost> host) {
        std::move(callback).Run();
        *out_host = std::move(host);
      },
      &host, run_loop.QuitClosure());
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](Gpu* gpu, TestGpuImpl* gpu_impl,
         gpu::GpuChannelEstablishedCallback callback,
         scoped_refptr<gpu::GpuChannelHost> host) {
        EXPECT_FALSE(host);
        // Responding to the first request would issue a second request
        // immediately which should succeed.
        gpu_impl->SetRequestWillSucceed(true);
        gpu->EstablishGpuChannel(std::move(callback));
      },
      gpu(), gpu_impl(), std::move(callback)));

  run_loop.Run();
  EXPECT_TRUE(host);
}

// Tests that if a request for a gpu channel succeeded, then subsequent requests
// are met synchronously.
TEST_F(GpuTest, EstablishRequestResponseSynchronouslyOnSuccess) {
  base::RunLoop run_loop;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](base::OnceClosure callback, scoped_refptr<gpu::GpuChannelHost> host) {
        EXPECT_TRUE(host);
        std::move(callback).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();

  int counter = 0;
  auto callback = base::BindOnce(
      [](int* counter, scoped_refptr<gpu::GpuChannelHost> host) {
        EXPECT_TRUE(host);
        ++(*counter);
      },
      &counter);
  gpu()->EstablishGpuChannel(std::move(callback));
  EXPECT_EQ(1, counter);
}

// Tests that if EstablishGpuChannel() was called but hasn't finished yet and
// EstablishGpuChannelSync() is called, that EstablishGpuChannelSync() blocks
// the thread until the original call returns.
TEST_F(GpuTest, EstablishRequestAsyncThenSync) {
  int counter = 0;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](int* counter, scoped_refptr<gpu::GpuChannelHost> host) {
        EXPECT_TRUE(host);
        ++(*counter);
      },
      base::Unretained(&counter)));

  scoped_refptr<gpu::GpuChannelHost> host = gpu()->EstablishGpuChannelSync();
  EXPECT_EQ(1, counter);
  EXPECT_TRUE(host);
}

// Tests that Gpu::EstablishGpuChannelSync() returns even if a connection error
// occurs. The implementation of mojom::Gpu never runs the callback for
// mojom::Gpu::EstablishGpuChannel() due to the connection error.
TEST_F(GpuTest, SyncConnectionError) {
  gpu_impl()->CloseBindingOnRequest();

  scoped_refptr<gpu::GpuChannelHost> channel = gpu()->EstablishGpuChannelSync();
  EXPECT_FALSE(channel);

  // Subsequent calls should also return.
  channel = gpu()->EstablishGpuChannelSync();
  EXPECT_FALSE(channel);
}

// Tests that Gpu::EstablishGpuChannel() callbacks are run even if a connection
// error occurs. The implementation of mojom::Gpu never runs the callback for
// mojom::Gpu::EstablishGpuChannel() due to the connection error.
TEST_F(GpuTest, AsyncConnectionError) {
  gpu_impl()->CloseBindingOnRequest();

  int counter = 2;
  base::RunLoop run_loop;
  // A callback that decrements the counter, and runs the callback when the
  // counter reaches 0.
  auto callback = base::BindRepeating(
      [](int* counter, const base::RepeatingClosure& callback,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        EXPECT_FALSE(channel);
        --(*counter);
        if (*counter == 0)
          callback.Run();
      },
      &counter, run_loop.QuitClosure());

  gpu()->EstablishGpuChannel(callback);
  gpu()->EstablishGpuChannel(callback);
  EXPECT_EQ(2, counter);
  run_loop.Run();
  EXPECT_EQ(0, counter);
}

// Tests that if EstablishGpuChannelSync() is called after a request for
// EstablishGpuChannel() has returned that request is used immediately.
TEST_F(GpuTest, EstablishRequestAsyncThenSyncWithResponse) {
  int counter = 0;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](int* counter, scoped_refptr<gpu::GpuChannelHost> host) {
        EXPECT_TRUE(host);
        ++(*counter);
      },
      base::Unretained(&counter)));

  BlockMainFlushIO();

  // Gpu will be in a state where a response for EstablishGpuChannel() has
  // been received on the IO thread and a task has been posted to the main
  // thread to process it. The main thread task hasn't run yet so the callback
  // won't have run.
  EXPECT_EQ(0, counter);

  // Run EstablishGpuChannelSync() before the posted task can run. The response
  // to the async request should be used immediately, the pending callback
  // should fire and then EstablishGpuChannelSync() should return.
  scoped_refptr<gpu::GpuChannelHost> host = gpu()->EstablishGpuChannelSync();
  EXPECT_EQ(1, counter);
  EXPECT_TRUE(host);

  // The original posted task will do nothing when it runs later.
}

// Tests that if Gpu is destroyed with a pending request it doesn't cause any
// issues.
TEST_F(GpuTest, DestroyGpuWithPendingRequest) {
  int counter = 0;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](int* counter, scoped_refptr<gpu::GpuChannelHost> host) {
        ++(*counter);
      },
      base::Unretained(&counter)));

  // This should cancel the pending request and not crash.
  DestroyGpu();

  // The callback shouldn't be called.
  FlushMainAndIO();
  EXPECT_EQ(0, counter);
}

class InitialGpuChannelTest : public GpuTest {
 public:
  InitialGpuChannelTest() = default;
  ~InitialGpuChannelTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kSendGPUChannelEarly, features::kRemoveGPULegacyIPC}, {});
    GpuTest::SetUp();
  }

  void TearDown() override {
    DestroyGpu();
    GpuTest::TearDown();
  }

  void SetupInitialChannel() {
    EXPECT_CALL(gpu_impl()->mock_gpu_channel_, GetGPUInfo(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly([](auto callback) {
          gpu::GPUInfo gpu_info;
          gpu_info.initialization_time = base::Milliseconds(1);
          gpu_info.gpu.vendor_id = 1;
          std::move(callback).Run(gpu_info, gpu::GpuFeatureInfo(),
                                  gpu::SharedImageCapabilities());
        });
    mojo::MessagePipe message_pipe;

    gpu_ = Gpu::Create(GetRemote(), io_thread().task_runner(), /*client_id=*/1,
                       std::move(message_pipe.handle0), base::NullCallback());

    io_thread().task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TestGpuImpl::BindGpuChannel,
                                  base::Unretained(gpu_impl()),
                                  std::move(message_pipe.handle1)));
    FlushMainAndIO();
  }
};

TEST_F(InitialGpuChannelTest, EstablishInitialChannelAsync) {
  SetupInitialChannel();
  base::RunLoop run_loop;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](base::OnceClosure closure,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        EXPECT_TRUE(channel);
        std::move(closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(InitialGpuChannelTest, EstablishInitialChannelSync) {
  SetupInitialChannel();
  scoped_refptr<gpu::GpuChannelHost> host = gpu()->EstablishGpuChannelSync();
  EXPECT_TRUE(host);
}

TEST_F(InitialGpuChannelTest, EstablishInitialChannelAsyncThenSync) {
  SetupInitialChannel();
  base::RunLoop run_loop;
  gpu()->EstablishGpuChannel(base::BindOnce(
      [](base::OnceClosure closure,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        EXPECT_TRUE(channel);
        std::move(closure).Run();
      },
      run_loop.QuitClosure()));

  scoped_refptr<gpu::GpuChannelHost> host = gpu()->EstablishGpuChannelSync();
  EXPECT_TRUE(host);
  run_loop.Run();
}

}  // namespace viz
