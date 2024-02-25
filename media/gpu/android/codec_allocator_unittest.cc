// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_allocator.h"

#include <stdint.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/base/subsample_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::ReturnRef;

namespace media {

class CodecAllocatorTest : public testing::Test {
 public:
  CodecAllocatorTest() : allocator_thread_("AllocatorThread") {
    // Don't start the clock at null.
    tick_clock_.Advance(base::Seconds(1));
    allocator_ = new CodecAllocator(
        base::BindRepeating(&MockMediaCodecBridge::CreateVideoDecoder),
        base::SequencedTaskRunner::GetCurrentDefault());
    allocator_->tick_clock_ = &tick_clock_;
  }

  CodecAllocatorTest(const CodecAllocatorTest&) = delete;
  CodecAllocatorTest& operator=(const CodecAllocatorTest&) = delete;

  ~CodecAllocatorTest() override {
    if (allocator_thread_.IsRunning()) {
      // Don't leave any threads hung, or this will hang too. It would be nice
      // if we could let a unique ptr handle this, but the destructor is
      // private.  We also have to destroy it on the right thread.
      allocator_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce([](CodecAllocator* allocator) { delete allocator; },
                         allocator_));

      allocator_thread_.Stop();
      return;
    }

    delete allocator_;
  }

  void CreateAllocatorOnAnotherThread() {
    delete allocator_;

    // Start a thread for the allocator.  This would normally be the GPU main
    // thread.
    CHECK(allocator_thread_.Start());
    allocator_ = new CodecAllocator(
        base::BindRepeating(&MockMediaCodecBridge::CreateVideoDecoder),
        allocator_thread_.task_runner());
    allocator_->tick_clock_ = &tick_clock_;
  }

  void OnCodecCreatedInternal(base::OnceClosure quit_closure,
                              std::unique_ptr<MediaCodecBridge> codec) {
    // This should always be called on the main thread, despite whatever thread
    // the allocator happens to be running on.
    ASSERT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());

    last_created_codec_.reset(
        reinterpret_cast<MockMediaCodecBridge*>(codec.release()));

    OnCodecCreated(last_created_codec_->GetCodecType());
    std::move(quit_closure).Run();
  }

  void OnCodecReleasedInternal(base::OnceClosure quit_closure) {
    // This should always be called on the main thread, despite whatever thread
    // the allocator happens to be running on.
    ASSERT_TRUE(
        task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    OnCodecReleased();
    std::move(quit_closure).Run();
  }

  bool IsPrimaryTaskRunnerLikelyHung() const {
    CHECK(!allocator_thread_.IsRunning());
    return allocator_->IsPrimaryTaskRunnerLikelyHung();
  }

  void VerifyOnPrimaryTaskRunner() {
    ASSERT_TRUE(allocator_->primary_task_runner_->RunsTasksInCurrentSequence());
  }

  void VerifyOnSecondaryTaskRunner() {
    ASSERT_TRUE(
        allocator_->secondary_task_runner_->RunsTasksInCurrentSequence());
  }

  MOCK_METHOD1(OnCodecCreated, void(CodecType));
  MOCK_METHOD0(OnCodecReleased, void());

  // Allocate and return a config that allows any codec, and is suitable for
  // hardware decode.
  std::unique_ptr<VideoCodecConfig> CreateConfig() {
    auto config = std::make_unique<VideoCodecConfig>();
    config->codec_type = CodecType::kAny;
    config->initial_expected_coded_size =
        CodecAllocator::kMinHardwareResolution;
    return config;
  }

 protected:
  // So that we can get the thread's task runner.
  base::test::TaskEnvironment task_environment_;

  base::Thread allocator_thread_;

  // The test params for |allocator_|.
  base::SimpleTestTickClock tick_clock_;

  // Allocators that we own. They are not unique_ptrs because the destructor is
  // private and they need to be destructed on the right thread.
  raw_ptr<CodecAllocator> allocator_ = nullptr;

  std::unique_ptr<MockMediaCodecBridge> last_created_codec_;
};

TEST_F(CodecAllocatorTest, NormalCreation) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  auto config = CreateConfig();

  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), run_loop.QuitClosure()),
      std::move(config));

  EXPECT_CALL(*this, OnCodecCreated(CodecType::kAny));
  run_loop.Run();
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, NormalSecureCreation) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  auto config = CreateConfig();
  config->codec_type = CodecType::kSecure;

  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), run_loop.QuitClosure()),
      std::move(config));

  EXPECT_CALL(*this, OnCodecCreated(CodecType::kSecure));
  run_loop.Run();
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, MultipleCreation) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  auto config = CreateConfig();

  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), base::DoNothing()),
      std::move(config));

  // Advance some time, but not enough to trigger hang detection.
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
  tick_clock_.Advance(base::Milliseconds(400));
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  auto config_secure = CreateConfig();
  config_secure->codec_type = CodecType::kSecure;

  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), run_loop.QuitClosure()),
      std::move(config_secure));

  EXPECT_CALL(*this, OnCodecCreated(CodecType::kAny));
  EXPECT_CALL(*this, OnCodecCreated(CodecType::kSecure));

  run_loop.Run();
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, MultipleRelease) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  base::RunLoop run_loop;
  allocator_->ReleaseMediaCodec(
      std::make_unique<MockMediaCodecBridge>(),
      base::BindOnce(&CodecAllocatorTest::OnCodecReleasedInternal,
                     base::Unretained(this), base::DoNothing()));

  // Advance some time, but not enough to trigger hang detection.
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
  tick_clock_.Advance(base::Milliseconds(400));
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  allocator_->ReleaseMediaCodec(
      std::make_unique<MockMediaCodecBridge>(),
      base::BindOnce(&CodecAllocatorTest::OnCodecReleasedInternal,
                     base::Unretained(this), run_loop.QuitClosure()));

  EXPECT_CALL(*this, OnCodecReleased()).Times(2);

  run_loop.Run();
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, StalledReleaseCountsAsHung) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  // Release null codec, but don't pump message loop.
  allocator_->ReleaseMediaCodec(std::make_unique<MockMediaCodecBridge>(),
                                base::DoNothing());
  tick_clock_.Advance(base::Seconds(1));
  ASSERT_TRUE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, StalledCreateCountsAsHung) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  // Create codec, but don't pump message loop.
  auto config = CreateConfig();
  config->codec_type = CodecType::kSecure;
  allocator_->CreateMediaCodecAsync(base::DoNothing(), std::move(config));
  tick_clock_.Advance(base::Seconds(1));
  ASSERT_TRUE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, SecureCreationFailsWhenHung) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  // Release null codec, but don't pump message loop.
  allocator_->ReleaseMediaCodec(std::make_unique<MockMediaCodecBridge>(),
                                base::DoNothing());
  tick_clock_.Advance(base::Seconds(1));
  ASSERT_TRUE(IsPrimaryTaskRunnerLikelyHung());

  // Secure creation should fail since we're now using software codecs.
  auto config = CreateConfig();
  config->codec_type = CodecType::kSecure;
  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<MediaCodecBridge> codec) {
            ASSERT_FALSE(codec);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()),
      std::move(config));
  run_loop.Run();

  // QuitClosure may run before the initial release processes, so RunUntilIdle
  // here such that hung status is cleared.
  task_environment_.RunUntilIdle();

  // Running the loop should clear hung status.
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

TEST_F(CodecAllocatorTest, SoftwareCodecUsedWhenHung) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  // Release null codec, but don't pump message loop.
  allocator_->ReleaseMediaCodec(std::make_unique<MockMediaCodecBridge>(),
                                base::DoNothing());
  tick_clock_.Advance(base::Seconds(1));
  ASSERT_TRUE(IsPrimaryTaskRunnerLikelyHung());

  // Creation should fall back to software.
  auto config = CreateConfig();
  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), run_loop.QuitClosure()),
      std::move(config));

  EXPECT_CALL(*this, OnCodecCreated(CodecType::kSoftware));
  run_loop.Run();

  // QuitClosure may run before the initial release processes, so RunUntilIdle
  // here such that hung status is cleared.
  task_environment_.RunUntilIdle();

  // Running the loop should clear hung status.
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

// Verifies that software codecs are released on the secondary task runner when
// hung and that non-sw codecs are always released on the primary task runner.
TEST_F(CodecAllocatorTest, CodecReleasedOnRightTaskRunnerWhenHung) {
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());

  // Release null codec, but don't pump message loop.
  allocator_->ReleaseMediaCodec(std::make_unique<MockMediaCodecBridge>(),
                                base::DoNothing());
  tick_clock_.Advance(base::Seconds(1));
  ASSERT_TRUE(IsPrimaryTaskRunnerLikelyHung());

  // Release software codec, ensure it runs on secondary task runner.
  auto config = CreateConfig();
  config->codec_type = CodecType::kSoftware;
  auto sw_codec = MockMediaCodecBridge::CreateVideoDecoder(*config);
  reinterpret_cast<MockMediaCodecBridge*>(sw_codec.get())
      ->destruction_cb.ReplaceClosure(
          base::BindOnce(&CodecAllocatorTest::VerifyOnSecondaryTaskRunner,
                         base::Unretained(this)));
  allocator_->ReleaseMediaCodec(std::move(sw_codec), base::DoNothing());

  // Release hardware codec, ensure it runs on primary task runner.
  config->codec_type = CodecType::kAny;
  auto hw_codec = MockMediaCodecBridge::CreateVideoDecoder(*config);
  reinterpret_cast<MockMediaCodecBridge*>(hw_codec.get())
      ->destruction_cb.ReplaceClosure(
          base::BindOnce(&CodecAllocatorTest::VerifyOnPrimaryTaskRunner,
                         base::Unretained(this)));
  allocator_->ReleaseMediaCodec(std::move(hw_codec), base::DoNothing());

  // Release secure (hardware) codec, ensure it runs on primary task runner.
  config->codec_type = CodecType::kSecure;
  auto secure_codec = MockMediaCodecBridge::CreateVideoDecoder(*config);
  reinterpret_cast<MockMediaCodecBridge*>(secure_codec.get())
      ->destruction_cb.ReplaceClosure(
          base::BindOnce(&CodecAllocatorTest::VerifyOnPrimaryTaskRunner,
                         base::Unretained(this)));
  allocator_->ReleaseMediaCodec(std::move(secure_codec), base::DoNothing());

  // QuitClosure may run before the initial release processes, so RunUntilIdle
  // here such that hung status is cleared.
  task_environment_.RunUntilIdle();

  // Running the loop should clear hung status.
  ASSERT_FALSE(IsPrimaryTaskRunnerLikelyHung());
}

// Make sure that allocating / freeing a codec on the allocator's thread
// completes, and doesn't DCHECK.
TEST_F(CodecAllocatorTest, AllocateAndDestroyCodecOnAllocatorThread) {
  CreateAllocatorOnAnotherThread();

  {
    base::RunLoop run_loop;
    auto config = CreateConfig();

    allocator_->CreateMediaCodecAsync(
        base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                       base::Unretained(this), run_loop.QuitClosure()),
        std::move(config));
    EXPECT_CALL(*this, OnCodecCreated(CodecType::kAny));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    allocator_->ReleaseMediaCodec(
        std::move(last_created_codec_),
        base::BindOnce(&CodecAllocatorTest::OnCodecReleasedInternal,
                       base::Unretained(this), run_loop.QuitClosure()));
    EXPECT_CALL(*this, OnCodecReleased());
    run_loop.Run();
  }
}

TEST_F(CodecAllocatorTest, LowResolutionGetsSoftware) {
  auto config = CreateConfig();
  config->initial_expected_coded_size =
      CodecAllocator::kMinHardwareResolution - gfx::Size(1, 1);
  base::RunLoop run_loop;
  allocator_->CreateMediaCodecAsync(
      base::BindOnce(&CodecAllocatorTest::OnCodecCreatedInternal,
                     base::Unretained(this), run_loop.QuitClosure()),
      std::move(config));

  EXPECT_CALL(*this, OnCodecCreated(CodecType::kSoftware));
  run_loop.Run();
}

}  // namespace media
