// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_allocator.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "media/base/android/mock_android_overlay.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::ReturnRef;
using testing::_;

namespace media {
namespace {
template <typename ReturnType>
void RunAndSignalTask(base::WaitableEvent* event,
                      ReturnType* return_value,
                      base::OnceCallback<ReturnType(void)> cb) {
  *return_value = std::move(cb).Run();
  event->Signal();
}

void WaitUntilRestarted(base::WaitableEvent* about_to_wait_event,
                        base::WaitableEvent* wait_event) {
  // Notify somebody that we've started.
  if (about_to_wait_event)
    about_to_wait_event->Signal();
  wait_event->Wait();
}

void SignalImmediately(base::WaitableEvent* event) {
  event->Signal();
}
}  // namespace

class MockClient : public CodecAllocatorClient {
 public:
  MockClient()
      : codec_arrived_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
        weak_factory_(this) {}

  // Gmock doesn't let us mock methods taking move-only types.
  MOCK_METHOD1(OnCodecConfiguredMock, void(MediaCodecBridge* media_codec));
  void OnCodecConfigured(
      std::unique_ptr<MediaCodecBridge> media_codec,
      scoped_refptr<AVDASurfaceBundle> surface_bundle) override {
    media_codec_ = std::move(media_codec);
    OnCodecConfiguredMock(media_codec.get());
    codec_arrived_event_.Signal();
  }

  base::WeakPtr<CodecAllocatorClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Most recently provided codec.
  std::unique_ptr<MediaCodecBridge> media_codec_;

  base::WaitableEvent codec_arrived_event_;

  base::WeakPtrFactory<CodecAllocatorClient> weak_factory_;
};

class CodecAllocatorTest : public testing::Test {
 public:
  CodecAllocatorTest()
      : allocator_thread_("AllocatorThread"),
        stop_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {
    // Don't start the clock at null.
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
  }

  ~CodecAllocatorTest() override {}

  // Utility fn to test out threading.
  void AllocateCodec() {
    allocator_->StartThread(avda1_);
    scoped_refptr<CodecConfig> codec_config(new CodecConfig);
    codec_config->surface_bundle = surface_bundle_;
    EXPECT_CALL(*avda1_, OnCodecConfiguredMock(_));
    allocator_->CreateMediaCodecAsync(avda1_->GetWeakPtr(), codec_config);
  }

  void DestroyCodec() {
    // Make sure that we got a codec.
    ASSERT_NE(avda1_->media_codec_, nullptr);
    base::WaitableEvent destruction_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    static_cast<MockMediaCodecBridge*>(avda1_->media_codec_.get())
        ->SetCodecDestroyedEvent(&destruction_event);
    allocator_->ReleaseMediaCodec(std::move(avda1_->media_codec_),
                                  surface_bundle_);

    // This won't wait for the threads to stop, which means that the release
    // might not have completed yet.  Even once we are signalled that the codec
    // has been destroyed, we can't be sure that OnMediaCodecReleased has run
    // on the allocator thread.  To get around this, one should wait on
    // |stop_event_|, but not here.  If we're run on the allocator's thread,
    // then that's where |stop_event_| will be signalled from.
    allocator_->StopThread(avda1_);
    // The codec destruction should be async with respect to us.
    destruction_event.Wait();

    // Important: we don't know that OnMediaCodecReleased has completed.
    // If we clean up the test and post the allocator's destruction to the
    // allocator thread, before the "and reply" posts the codec release, then
    // the codec release will be run on a destructed allocator.  Either we
    // should synchronize on that, or quit using base::Unretained().
    // Waiting for |stop_event_| and then for |allocator_thread_| should be
    // sufficent to avoid this.
    // Waiting for the overlay to be released is probably also enough, since
    // that happens to be run on OnMediaCodecReleased also.
  }

  void WaitForSurfaceDestruction() {
    // This may be called from any thread.
    PostAndWait(FROM_HERE,
                base::BindOnce(
                    [](CodecAllocator* allocator, AndroidOverlay* overlay) {
                      allocator->WaitForPendingReleaseForTesting(overlay);
                      return true;
                    },
                    allocator_, surface_bundle_->overlay.get()));
  }

 protected:
  void SetUp() override {
    // Start the main thread for the allocator.  This would normally be the GPU
    // main thread.
    ASSERT_TRUE(allocator_thread_.Start());

    CodecAllocator::CodecFactoryCB factory_cb(
        base::BindRepeating(&MockMediaCodecBridge::CreateVideoDecoder));

    // Create the first allocator on the allocator thread.
    allocator_ = PostAndWait(
        FROM_HERE,
        base::BindOnce(
            [](CodecAllocator::CodecFactoryCB factory_cb,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               const base::TickClock* clock, base::WaitableEvent* event) {
              return new CodecAllocator(factory_cb, task_runner, clock, event);
            },
            factory_cb, allocator_thread_.task_runner(), &tick_clock_,
            &stop_event_));
    allocator2_ =
        new CodecAllocator(factory_cb, base::SequencedTaskRunnerHandle::Get());

    // Create a SurfaceBundle that provides an overlay.  It will provide a null
    // java ref if requested.
    std::unique_ptr<MockAndroidOverlay> overlay =
        std::make_unique<NiceMock<MockAndroidOverlay>>();
    scoped_refptr<CodecConfig> codec_config(new CodecConfig);
    ON_CALL(*overlay, GetJavaSurface())
        .WillByDefault(ReturnRef(null_java_ref_));
    surface_bundle_ = new AVDASurfaceBundle(std::move(overlay));
  }

  void TearDown() override {
    // Don't leave any threads hung, or this will hang too.
    // It would be nice if we could let a unique ptr handle this, but the
    // destructor is private.  We also have to destroy it on the right thread.
    PostAndWait(FROM_HERE, base::BindOnce(
                               [](CodecAllocator* allocator) {
                                 delete allocator;
                                 return true;
                               },
                               allocator_));

    allocator_thread_.Stop();
    delete allocator2_;
  }

 protected:
  // Start / stop the threads for |avda| on the right thread.
  void StartThread(CodecAllocatorClient* avda) {
    PostAndWait(FROM_HERE,
                base::BindOnce(
                    [](CodecAllocator* allocator, CodecAllocatorClient* avda) {
                      allocator->StartThread(avda);
                      return true;  // void won't work.
                    },
                    allocator_, avda));
  }

  void StopThread(CodecAllocatorClient* avda) {
    // Note that we also wait for the stop event, so that we know that the
    // stop has completed.  It's async with respect to the allocator thread.
    PostAndWait(FROM_HERE,
                base::BindOnce(
                    [](CodecAllocator* allocator, CodecAllocatorClient* avda) {
                      allocator->StopThread(avda);
                      return true;
                    },
                    allocator_, avda));
    // Note that we don't do this on the allocator thread, since that's the
    // thread that will signal it.
    stop_event_.Wait();
  }

  // Return the running state of |task_type|, doing the necessary thread hops.
  bool IsThreadRunning(TaskType task_type) {
    return PostAndWait(
        FROM_HERE,
        base::BindOnce(
            [](CodecAllocator* allocator, TaskType task_type) {
              return allocator->GetThreadForTesting(task_type).IsRunning();
            },
            allocator_, task_type));
  }

  base::Optional<TaskType> TaskTypeForAllocation(
      bool software_codec_forbidden) {
    return PostAndWait(
        FROM_HERE,
        base::BindOnce(&CodecAllocator::TaskTypeForAllocation,
                       base::Unretained(allocator_), software_codec_forbidden));
  }

  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(
      TaskType task_type) {
    return PostAndWait(FROM_HERE,
                       base::BindOnce(&CodecAllocator::TaskRunnerFor,
                                      base::Unretained(allocator_), task_type));
  }

  // Post |cb| to the allocator thread, and wait for a response.  Note that we
  // don't have a specialization for void, and void won't work as written.  So,
  // be sure to return something.
  template <typename ReturnType>
  ReturnType PostAndWait(const base::Location& from_here,
                         base::OnceCallback<ReturnType(void)> cb) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    ReturnType return_value = ReturnType();
    allocator_thread_.task_runner()->PostTask(
        from_here, base::BindOnce(&RunAndSignalTask<ReturnType>, &event,
                                  &return_value, std::move(cb)));
    event.Wait();
    return return_value;
  }

  // So that we can get the thread's task runner.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  base::Thread allocator_thread_;

  // The test params for |allocator_|.
  base::SimpleTestTickClock tick_clock_;
  base::WaitableEvent stop_event_;

  // Allocators that we own. The first is intialized to be used on the allocator
  // thread and the second one is initialized on the test thread. Each test
  // should only be using one of the two. They are not unique_ptrs because the
  // destructor is private and they need to be destructed on the right thread.
  CodecAllocator* allocator_ = nullptr;
  CodecAllocator* allocator2_ = nullptr;

  NiceMock<MockClient> client1_, client2_, client3_;
  NiceMock<MockClient>* avda1_ = &client1_;
  NiceMock<MockClient>* avda2_ = &client2_;
  NiceMock<MockClient>* avda3_ = &client3_;

  // Surface bundle that has an overlay.
  scoped_refptr<AVDASurfaceBundle> surface_bundle_;
  base::android::JavaRef<jobject> null_java_ref_;
};

TEST_F(CodecAllocatorTest, ThreadsStartWhenClientsStart) {
  ASSERT_FALSE(IsThreadRunning(AUTO_CODEC));
  ASSERT_FALSE(IsThreadRunning(SW_CODEC));
  StartThread(avda1_);
  ASSERT_TRUE(IsThreadRunning(AUTO_CODEC));
  ASSERT_TRUE(IsThreadRunning(SW_CODEC));
}

TEST_F(CodecAllocatorTest, ThreadsStopAfterAllClientsStop) {
  StartThread(avda1_);
  StartThread(avda2_);
  StopThread(avda1_);
  ASSERT_TRUE(IsThreadRunning(AUTO_CODEC));
  StopThread(avda2_);
  ASSERT_FALSE(IsThreadRunning(AUTO_CODEC));
  ASSERT_FALSE(IsThreadRunning(SW_CODEC));
}

TEST_F(CodecAllocatorTest, TestHangThread) {
  StartThread(avda1_);
  ASSERT_EQ(AUTO_CODEC, TaskTypeForAllocation(false));

  // Hang the AUTO_CODEC thread.
  base::WaitableEvent about_to_wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(AUTO_CODEC)
      ->PostTask(FROM_HERE, base::BindOnce(&WaitUntilRestarted,
                                           &about_to_wait_event, &wait_event));
  // Wait until the task starts, so that |allocator_| starts the hang timer.
  about_to_wait_event.Wait();

  // Verify that we've failed over after a long time has passed.
  tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(SW_CODEC, TaskTypeForAllocation(false));

  // Un-hang the thread and wait for it to let another task run.  This will
  // notify |allocator_| that the thread is no longer hung.
  base::WaitableEvent done_waiting_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(AUTO_CODEC)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SignalImmediately, &done_waiting_event));
  wait_event.Signal();
  done_waiting_event.Wait();

  // Verify that we've un-failed over.
  ASSERT_EQ(AUTO_CODEC, TaskTypeForAllocation(false));
}

TEST_F(CodecAllocatorTest, AllocateAndDestroyCodecOnAllocatorThread) {
  // Make sure that allocating / freeing a codec on the allocator's thread
  // completes, and doesn't DCHECK.
  allocator_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CodecAllocatorTest::AllocateCodec,
                                base::Unretained(this)));

  // Wait for the codec on this thread, rather than the allocator thread, since
  // that's where the codec will be posted.
  avda1_->codec_arrived_event_.Wait();

  allocator_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CodecAllocatorTest::DestroyCodec,
                                base::Unretained(this)));

  // Note that TearDown will join |allocator_thread_|.
  WaitForSurfaceDestruction();

  // Wait for threads to stop, now that we're not on the allocator thread.
  stop_event_.Wait();
}

TEST_F(CodecAllocatorTest, AllocateAndDestroyCodecOnNewThread) {
  // Make sure that allocating / freeing a codec on a random thread completes,
  // and doesn't DCHECK.
  base::Thread new_thread("NewThreadForTesting");
  ASSERT_TRUE(new_thread.Start());
  new_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CodecAllocatorTest::AllocateCodec,
                                base::Unretained(this)));

  // Wait for the codec on this thread, rather than |new_thread|, since that's
  // where the codec will be posted.
  avda1_->codec_arrived_event_.Wait();

  new_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CodecAllocatorTest::DestroyCodec,
                                base::Unretained(this)));
  new_thread.Stop();
  WaitForSurfaceDestruction();
  stop_event_.Wait();
}

}  // namespace media
